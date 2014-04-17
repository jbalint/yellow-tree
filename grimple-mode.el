;; This buffer is for notes you don't want to save, and for Lisp evaluation.
;; If you want to create a file, visit that file with C-x C-f,
;; then enter the text in that file's own buffer.



(setq grimple-font-lock-keywords
	  ;; not necessarily exhaustive
	  ;; not sure if while, try, etc are keywords in grimple
  '(("\\b\\(break\\|case\\|catch\\|class\\|do\\|extends\\|finally\\|for\\|from\\|goto\\|if\\|implements\\|import\\|new\\|package\\|private\\|protected\\|public\\|return\\|static\\|staticinvoke\\|switch\\|synchronized\\|throw\\|throws\\|to\\|try\\|with\\|while\\)\\b" . font-lock-function-name-face)
	;; TODO labels, strings
	("\\b\\(boolean\\|char\\|double\\|float\\|int\\|long\\|short\\|void\\)\\b" . font-lock-type-face)
	("\\b\\(null\\)\\b" . font-lock-constant-face)
	("\\blabel[[:digit:]]+\\b" . font-lock-constant-face)
	("com\\.*\b" . font-lock-type-face)))

;; copied from archive-tmpdir (arc-mode.el)
(defcustom grimple-tmpdir
  ;; make-temp-name is safe here because we use this name
  ;; to create a directory.
  (make-temp-name
   (expand-file-name ;;(if (eq system-type 'ms-dos) "ar" "archive.tmp")
		     temporary-file-directory))
  "Directory for temporary files made by `grimple-mode.el'."
  :type 'directory
  :group 'archive)

(put 'grimple-class-mode 'mode-class 'special)

(defun grimple-class-name-of-file (file-name)
  "")

(defun grimple-decompile-class (file-name)
  (let* ((class-name (replace-regexp-in-string "pattern" "replacement" file-name))
		  (args '("-jar" grimple-soot-jar "-cp" "CLASSPATH" "-f" "grimple" class-name)))
	(call-process "java" nil nil nil args)))

(defun grimple-class-mode ()
  (grimple-mode)
  (kill-all-local-variables)
  (setq major-mode 'grimple-class-mode)
  (setq mode-name "Grimple")
  (set (make-local-variable 'font-lock-defaults)
	   '(grimple-font-lock-keywords nil nil nil nil))
  (grimple-decompile-class buffer-file-name)
  (font-lock-mode nil)
  (turn-on-font-lock))

(defun grimple-mode ()
  (interactive)
  (kill-all-local-variables)
  (setq major-mode 'grimple-mode)
  (setq mode-name "Grimple")
  (set (make-local-variable 'font-lock-defaults)
	   '(grimple-font-lock-keywords nil nil nil nil))
  (font-lock-mode nil)
  (turn-on-font-lock))

(add-to-list 'auto-mode-alist '("\\.grimp\\(le\\)?\\'" . grimple-mode))
(add-to-list 'auto-mode-alist '("\\.class\\'" . grimple-class-mode))

(provide 'grimple-mode)
