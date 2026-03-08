
`MARU_ENABLE_INTERNAL_CHECKS` has a few potentially surprising effects. They are listed here

## X11

`MARU_ENABLE_INTERNAL_CHECKS` will register a global error callback in X11. If you want to enbale `MARU_ENABLE_INTERNAL_CHECKS` in a project that uses multiple concurrent contexts, you may want to comment that out.