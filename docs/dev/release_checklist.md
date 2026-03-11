# Release Checklist

Use this checklist before cutting a release.

## API Contracts

- Audit [`docs/user/api_contracts.md`](../user/api_contracts.md) against `src/core/maru_api_constraints.h`.
- If `src/core/maru_api_constraints.h` changed since the last release, verify that all user-visible contract changes are reflected in the relevant user docs and public header comments.
- Check that any new owner-thread exceptions or new ready/lost preconditions are documented explicitly.

## Public Docs

- Re-scan user guides that encode API semantics directly, especially Vulkan, monitors, data exchange, and backend/native-handle docs.
- Remove stale wording that contradicts current validation behavior.
