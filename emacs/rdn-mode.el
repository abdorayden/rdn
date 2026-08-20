;;; rdn-mode.el --- Major mode for editing Raden source code -*- lexical-binding: t -*-

;; Copyright (c) 2023-2026 Ray Den
;;
;; Permission is hereby granted, free of charge, to any person obtaining a copy
;; of this software and associated documentation files (the "Software"), to deal
;; in the Software without restriction, including without limitation the rights
;; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
;; copies of the Software, and to permit persons to whom the Software is
;; furnished to do so, subject to the following conditions:
;;
;; The above copyright notice and this permission notice shall be included in
;; all copies or substantial portions of the Software.
;;
;; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
;; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
;; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
;; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
;; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
;; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
;; THE SOFTWARE.

;; Author: Rayden
;; URL: https://github.com/abdorayden/rdn

(defconst rdn-mode-syntax-table
  (with-syntax-table (copy-syntax-table)
    (modify-syntax-entry ?\[ ". 1n")
    (modify-syntax-entry ?* ". 23n")
    (modify-syntax-entry ?\] ". 4n")
    (modify-syntax-entry ?\" "\"")
    (syntax-table))
  "Syntax table for `rdn-mode'.")

(eval-and-compile
  (defconst rdn-keywords
    '("if" "else" "end" "loop" "break" "continue"
      "let" "set" "const" "enum" "reset"
      "demac" "defun" "apply" "call" "pcall"
      "unlet" "ret" "module" "open")
    "Raden language keywords.")

  (defconst rdn-builtins
    '("print" "pop" "dup" "swap" "type" "to_string" "exit"
      "load" "loadnative" "add_load_path" "add_native_path"
      "error" "do_string" "do_file" "match"
      "assert" "append" "remove" "index" "len"
      "__argv" "__host_os" "__sharedlib_ext"
      "__line_col" "__file" "__func_name" "__stack_size")
    "Raden builtin words.")

  (defconst rdn-operators
    '("+" "-" "*" "/" "=" "!=" "<" ">" "<=" ">="
      "!" "|" "&" "^" "<<" ">>")
    "Raden builtin operators.")

  (defconst rdn-constants
    '("true" "false" "null")
    "Raden literal constants.")

  (defconst rdn-number-regexp
    (rx (opt (any "+-"))
        (or (seq (one-or-more digit) "."
                 (one-or-more digit)
                 (opt (seq (any "eE") (opt (any "+-"))
                           (one-or-more digit))))
            (seq (one-or-more digit) (any "eE")
                 (opt (any "+-")) (one-or-more digit))
            (seq "0x" (one-or-more hex-digit))
            (seq "0b" (one-or-more (any "01")))
            (seq "0o" (one-or-more (any "0-7")))
            (one-or-more digit)))
    "Regexp matching Raden numbers and floats."))

(defface rdn-escape-face
  '((((class color)) :inherit font-lock-regexp-grouping-backslash)
    (t :inherit font-lock-string-face :weight bold))
  "Face used for escape sequences inside strings."
  :group 'font-lock-faces)

(defvar rdn-escape-face 'rdn-escape-face
  "Face used for escape sequences inside strings.")

(defun rdn-font-lock-match-escape (limit)
  "Match an escape sequence at point when inside a string."
  (catch 'rdn-done
    (while (re-search-forward (rx "\\" (any ?n ?r ?t ?\" ?\\)) limit t)
      (when (nth 3 (syntax-ppss))
        (throw 'rdn-done (point))))
    nil))

(defconst rdn-highlights
  `((,rdn-number-regexp . font-lock-constant-face)
    (rdn-font-lock-match-escape 0 rdn-escape-face t)
    (,(regexp-opt rdn-keywords 'symbols) . font-lock-keyword-face)
    (,(regexp-opt rdn-builtins 'symbols) . font-lock-builtin-face)
    (,(regexp-opt rdn-operators) . font-lock-builtin-face)
    (,(regexp-opt rdn-constants 'symbols) . font-lock-constant-face)
    (,(rx (any "()")) . font-lock-builtin-face))
  "Font-lock rules for `rdn-mode'.")

;;;autoload
(define-derived-mode rdn-mode prog-mode "rdn"
  "Major mode for editing Raden source code."
  :syntax-table rdn-mode-syntax-table
  (setq font-lock-defaults '(rdn-highlights))
  (setq-local comment-start "[* ")
  (setq-local comment-end " *]"))

(add-to-list 'auto-mode-alist '("\\.rdn\\'" . rdn-mode))

(provide 'rdn-mode)
