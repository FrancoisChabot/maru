#include "utest.h"

#include "linux/x11/x11_internal.h"

#include <string.h>

static int g_xsend_event_count;
static XEvent g_last_xsend_event;
static int g_data_requested_count;

static int test_xsend_event(Display *display, Window window, Bool propagate,
                            long event_mask, XEvent *event_send) {
  (void)display;
  (void)window;
  (void)propagate;
  (void)event_mask;
  g_xsend_event_count++;
  g_last_xsend_event = *event_send;
  return Success;
}

static void test_event_callback(MARU_EventId type, MARU_Window *window,
                                const MARU_Event *evt, void *userdata) {
  (void)window;
  (void)evt;
  (void)userdata;
  if (type == MARU_EVENT_DATA_REQUESTED) {
    g_data_requested_count++;
  }
}

UTEST(X11DataExchange, SelectionNotifyIgnoresUnknownSelectionAtoms) {
  MARU_Context_X11 ctx;
  MARU_Window_X11 window;
  XEvent ev;

  memset(&ctx, 0, sizeof(ctx));
  memset(&window, 0, sizeof(window));
  memset(&ev, 0, sizeof(ev));

  ctx.selection_clipboard = 11;
  ctx.selection_primary = 12;
  ctx.xdnd_selection = 13;

  ctx.dnd_request.pending = true;
  ctx.dnd_request.window = &window;
  ctx.dnd_request.target_atom = 33;
  ctx.dnd_request.property_atom = None;
  ctx.dnd_request.userdata = (void *)0x1234;
  window.handle = 77;

  ev.type = SelectionNotify;
  ev.xselection.requestor = window.handle;
  ev.xselection.selection = 99;
  ev.xselection.property = 55;

  ASSERT_TRUE(_maru_x11_process_dataexchange_event(&ctx, &ev));
  EXPECT_TRUE(ctx.dnd_request.pending);
  EXPECT_EQ(ctx.dnd_request.window, &window);
  EXPECT_EQ(ctx.dnd_request.property_atom, None);
  EXPECT_EQ(ctx.dnd_request.userdata, (void *)0x1234);
}

UTEST(X11DataExchange, SelectionRequestRejectsUnknownSelectionAtomsWithoutDndDispatch) {
  MARU_Context_X11 ctx;
  MARU_Window_X11 owner_window;
  MARU_PumpContext pump_ctx;
  XEvent ev;

  memset(&ctx, 0, sizeof(ctx));
  memset(&owner_window, 0, sizeof(owner_window));
  memset(&pump_ctx, 0, sizeof(pump_ctx));
  memset(&ev, 0, sizeof(ev));
  memset(&g_last_xsend_event, 0, sizeof(g_last_xsend_event));
  g_xsend_event_count = 0;
  g_data_requested_count = 0;

  ctx.selection_clipboard = 11;
  ctx.selection_primary = 12;
  ctx.xdnd_selection = 13;
  ctx.x11_lib.XSendEvent = test_xsend_event;
  ctx.base.pump_ctx = &pump_ctx;

  pump_ctx.mask = MARU_ALL_EVENTS;
  pump_ctx.callback = test_event_callback;

  owner_window.handle = 77;
  ctx.dnd_offer.owner_window = &owner_window;

  ev.type = SelectionRequest;
  ev.xselectionrequest.owner = owner_window.handle;
  ev.xselectionrequest.requestor = 88;
  ev.xselectionrequest.selection = 99;
  ev.xselectionrequest.target = 44;
  ev.xselectionrequest.property = 55;
  ev.xselectionrequest.time = CurrentTime;

  ASSERT_TRUE(_maru_x11_process_dataexchange_event(&ctx, &ev));
  EXPECT_EQ(g_xsend_event_count, 1);
  EXPECT_EQ(g_last_xsend_event.type, SelectionNotify);
  EXPECT_EQ(g_last_xsend_event.xselection.requestor, ev.xselectionrequest.requestor);
  EXPECT_EQ(g_last_xsend_event.xselection.selection, ev.xselectionrequest.selection);
  EXPECT_EQ(g_last_xsend_event.xselection.property, None);
  EXPECT_EQ(g_data_requested_count, 0);
}

UTEST(X11DataExchange, DropActionNoneMapsToNoneAtom) {
  MARU_Context_X11 ctx;

  memset(&ctx, 0, sizeof(ctx));
  ctx.xdnd_action_copy = 21;
  ctx.xdnd_action_move = 22;
  ctx.xdnd_action_link = 23;

  EXPECT_EQ(_maru_x11_drop_action_to_atom(&ctx, MARU_DROP_ACTION_NONE), None);
  EXPECT_EQ(_maru_x11_drop_action_to_atom(&ctx, MARU_DROP_ACTION_COPY),
            ctx.xdnd_action_copy);
  EXPECT_EQ(_maru_x11_drop_action_to_atom(&ctx, MARU_DROP_ACTION_MOVE),
            ctx.xdnd_action_move);
  EXPECT_EQ(_maru_x11_drop_action_to_atom(&ctx, MARU_DROP_ACTION_LINK),
            ctx.xdnd_action_link);
}

UTEST(X11DataExchange, RejectedXdndStatusAdvertisesNoAction) {
  MARU_Context_X11 ctx;
  MARU_Window_X11 target_window;
  MARU_X11DnDSession session;

  memset(&ctx, 0, sizeof(ctx));
  memset(&target_window, 0, sizeof(target_window));
  memset(&session, 0, sizeof(session));
  memset(&g_last_xsend_event, 0, sizeof(g_last_xsend_event));
  g_xsend_event_count = 0;

  ctx.x11_lib.XSendEvent = test_xsend_event;
  ctx.xdnd_status = 101;
  ctx.xdnd_action_copy = 201;
  ctx.xdnd_action_move = 202;
  ctx.xdnd_action_link = 203;

  target_window.handle = 77;
  session.source_window = 88;
  session.target_window = &target_window;
  session.selected_action = MARU_DROP_ACTION_COPY;

  _maru_x11_send_xdnd_status(&ctx, &session, false);

  EXPECT_EQ(g_xsend_event_count, 1);
  EXPECT_EQ(g_last_xsend_event.xclient.message_type, ctx.xdnd_status);
  EXPECT_EQ(g_last_xsend_event.xclient.data.l[1], 0);
  EXPECT_EQ(g_last_xsend_event.xclient.data.l[4], (long)None);
}

UTEST(X11DataExchange, RejectedXdndFinishedAdvertisesNoAction) {
  MARU_Context_X11 ctx;
  MARU_Window_X11 target_window;
  MARU_X11DnDSession session;

  memset(&ctx, 0, sizeof(ctx));
  memset(&target_window, 0, sizeof(target_window));
  memset(&session, 0, sizeof(session));
  memset(&g_last_xsend_event, 0, sizeof(g_last_xsend_event));
  g_xsend_event_count = 0;

  ctx.x11_lib.XSendEvent = test_xsend_event;
  ctx.xdnd_finished = 102;
  ctx.xdnd_action_copy = 201;
  ctx.xdnd_action_move = 202;
  ctx.xdnd_action_link = 203;

  target_window.handle = 77;
  session.source_window = 88;
  session.target_window = &target_window;
  session.selected_action = MARU_DROP_ACTION_MOVE;

  _maru_x11_send_xdnd_finished(&ctx, &session, false);

  EXPECT_EQ(g_xsend_event_count, 1);
  EXPECT_EQ(g_last_xsend_event.xclient.message_type, ctx.xdnd_finished);
  EXPECT_EQ(g_last_xsend_event.xclient.data.l[1], 0);
  EXPECT_EQ(g_last_xsend_event.xclient.data.l[2], (long)None);
}
