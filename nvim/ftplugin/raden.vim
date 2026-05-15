setlocal commentstring=[*\ %s\ *]
setlocal comments=s1:[*,mb:*,ex:*]
setlocal suffixesadd=.rdn
setlocal formatoptions-=t
setlocal formatoptions+=croql
setlocal iskeyword+=_

let b:undo_ftplugin = "setlocal commentstring< comments< suffixesadd< formatoptions< iskeyword<"
