if exists("b:current_syntax")
  finish
endif

syntax case match

syntax region radenComment start=/\[\*/ end=/\*\]/ keepend

syntax region radenString start=/"/ skip=/\\./ end=/"/ keepend contains=radenEscape
syntax match radenEscape /\\["\\nrt]/

syntax match radenNumber /\v[-+]?\d+/
syntax match radenNumber /\v[-+]?0x\x+/
syntax match radenNumber /\v[-+]?0b[01]+/
syntax match radenNumber /\v[-+]?0o[0-7]+/
syntax match radenFloat /\v[-+]?\d+\.\d+([eE][-+]?\d+)?/
syntax match radenFloat /\v[-+]?\d+[eE][-+]?\d+/

syntax keyword radenNull null
syntax keyword radenBoolean true false

syntax keyword radenConditional if else end
syntax keyword radenRepeat loop break continue
syntax keyword radenKeyword let const enum reset defun apply call set pcall unlet

syntax keyword radenBuiltin print type exit pop swap dup to_string load loadnative add_load_path add_native_path error
syntax keyword radenBuiltin append remove index len __argv __host_os __sharedlib_ext

syntax match radenOperator /\V+/
syntax match radenOperator /-/
syntax match radenOperator /\V*/
syntax match radenOperator /\V\//
syntax match radenOperator /<<\|>>/
syntax match radenOperator /<=\|>=\|!=\|=\|<\|>/
syntax match radenOperator /[|&^!]/

syntax match radenListDelimiter /[()]/

highlight default link radenComment Comment
highlight default link radenString String
highlight default link radenEscape SpecialChar
highlight default link radenNumber Number
highlight default link radenFloat Float
highlight default link radenNull Constant
highlight default link radenBoolean Boolean
highlight default link radenConditional Conditional
highlight default link radenRepeat Repeat
highlight default link radenKeyword Keyword
highlight default link radenBuiltin Function
highlight default link radenOperator Operator
highlight default link radenListDelimiter Delimiter

let b:current_syntax = "raden"
