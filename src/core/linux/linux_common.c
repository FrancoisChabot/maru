#define _GNU_SOURCE
#include "linux_internal.h"
#include "dlib/linux_loader.h"
#include <unistd.h>
#include <sys/eventfd.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

static void _maru_linux_worker_process_device(MARU_Context_Linux_Common* common, struct udev_device* dev, const char* action) {
  const char* devnode = common->worker.udev_lib.udev_device_get_devnode(dev);

  if (devnode) {
    if (!action || strcmp(action, "add") == 0) {
      // TODO: Blocking open and capability check here.
    } else if (action && strcmp(action, "remove") == 0) {
      // TODO: Handle removal
    }
  }
}

static void _maru_linux_worker_process_udev_monitor(MARU_Context_Linux_Common* common) {
  struct udev_device* dev = common->worker.udev_lib.udev_monitor_receive_device(common->worker.udev_monitor);
  if (!dev) return;

  const char* action = common->worker.udev_lib.udev_device_get_action(dev);
  _maru_linux_worker_process_device(common, dev, action);

  common->worker.udev_lib.udev_device_unref(dev);
}

static void* _maru_linux_worker_main(void* arg) {
  MARU_Context_Linux_Common* common = (MARU_Context_Linux_Common*)arg;
  
  struct pollfd pfds[2];
  pfds[0].fd = common->worker.event_fd;
  pfds[0].events = POLLIN;
  pfds[1].fd = common->worker.udev_fd;
  pfds[1].events = POLLIN;

  nfds_t nfds = (common->worker.udev_fd >= 0) ? 2 : 1;

  bool terminate = false;
  while (!terminate) {
    int ret = poll(pfds, nfds, -1);
    if (ret < 0) {
      if (errno == EINTR || errno == EAGAIN) continue;
      break;
    }

    if (pfds[0].revents & POLLIN) {
      uint64_t val;
      read(common->worker.event_fd, &val, sizeof(val));

      if (atomic_exchange(&common->worker.has_message, false)) {
        MARU_LinuxWorkerMessage msg = atomic_load(&common->worker.message);
        switch (msg) {
          case MARU_LINUX_WORKER_MSG_TERMINATE:
            terminate = true;
            break;
        }
      }
    }

    if (nfds > 1 && (pfds[1].revents & POLLIN)) {
      _maru_linux_worker_process_udev_monitor(common);
    }
  }

  return NULL;
}

bool _maru_linux_common_init(MARU_Context_Linux_Common* common) {
  common->worker.event_fd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
  if (common->worker.event_fd < 0) {
    return false;
  }

  common->worker.udev = NULL;
  common->worker.udev_monitor = NULL;
  common->worker.udev_fd = -1;

  if (maru_linux_udev_load(NULL, &common->worker.udev_lib)) {
    common->worker.udev = common->worker.udev_lib.udev_new();
    if (common->worker.udev) {
      common->worker.udev_monitor = common->worker.udev_lib.udev_monitor_new_from_netlink(common->worker.udev, "udev");
      if (common->worker.udev_monitor) {
        common->worker.udev_lib.udev_monitor_filter_add_match_subsystem_devtype(common->worker.udev_monitor, "input", NULL);
        common->worker.udev_lib.udev_monitor_enable_receiving(common->worker.udev_monitor);
        common->worker.udev_fd = common->worker.udev_lib.udev_monitor_get_fd(common->worker.udev_monitor);
      }

      // Initial enumeration
      struct udev_enumerate* enumerate = common->worker.udev_lib.udev_enumerate_new(common->worker.udev);
      if (enumerate) {
        common->worker.udev_lib.udev_enumerate_add_match_subsystem(enumerate, "input");
        common->worker.udev_lib.udev_enumerate_scan_devices(enumerate);
        
        struct udev_list_entry* devices = common->worker.udev_lib.udev_enumerate_get_list_entry(enumerate);
        struct udev_list_entry* entry;

        for (entry = devices; entry != NULL; entry = common->worker.udev_lib.udev_list_entry_get_next(entry)) {
          const char* path = common->worker.udev_lib.udev_list_entry_get_name(entry);
          struct udev_device* dev = common->worker.udev_lib.udev_device_new_from_syspath(common->worker.udev, path);
          if (dev) {
            _maru_linux_worker_process_device(common, dev, NULL);
            common->worker.udev_lib.udev_device_unref(dev);
          }
        }
        common->worker.udev_lib.udev_enumerate_unref(enumerate);
      }
    }
  }

  atomic_init(&common->worker.has_message, false);

  if (pthread_create(&common->worker.thread, NULL, _maru_linux_worker_main, common) != 0) {
    _maru_linux_common_cleanup(common);
    return false;
  }

  return true;
}

void _maru_linux_common_cleanup(MARU_Context_Linux_Common* common) {
  if (common->worker.event_fd >= 0) {
    atomic_store(&common->worker.message, MARU_LINUX_WORKER_MSG_TERMINATE);
    atomic_store(&common->worker.has_message, true);
    
    uint64_t val = 1;
    write(common->worker.event_fd, &val, sizeof(val));

    pthread_join(common->worker.thread, NULL);
    
    close(common->worker.event_fd);
    common->worker.event_fd = -1;
  }

  if (common->worker.udev_monitor) {
    common->worker.udev_lib.udev_monitor_unref(common->worker.udev_monitor);
    common->worker.udev_monitor = NULL;
  }
  if (common->worker.udev) {
    common->worker.udev_lib.udev_unref(common->worker.udev);
    common->worker.udev = NULL;
  }
  if (common->worker.udev_lib.base.available) {
    maru_linux_udev_unload(&common->worker.udev_lib);
  }
}
