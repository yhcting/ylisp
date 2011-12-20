(set 'yhc.topdir (concat (getenv "PWD") "/"))
(set 'yhc.target "ylisp")
(setenv "GTAGSROOT" yhc.topdir)
(setenv "GTAGSDBPATH" yhc.topdir)

(load-file "~/.emacs-cedet.el")

(defun yhc.compile ()
    "Using make file in top directory"
    (interactive)
    (let ((cmd ""))
        (set 'cmd (concat "cd " yhc.topdir
                          "; make DEFS=\"-D__LISPTEST__ -D__LISPDEBUG__\""))
        ;(message cmd)))
        (compile cmd)))

(defun yhc.gdb ()
    "gdb wrapper"
    (interactive)
    (let ((cmd ""))
        (set 'cmd (concat "gdb --annotate=3 " yhc.topdir yhc.target))
        ;(message cmd)))
        (gdb cmd)))

;; override ede's key map for compile...
(global-set-key [f9] 'yhc.compile)
(global-set-key [f8] 'yhc.gdb)

(ede-cpp-root-project (concat yhc.target "-root")
                :name (concat yhc.target "-root")
                :file (concat "~/dev/cedetws/ylisp/" yhc.target "/Makefile")
                :include-path '("../include"
                               )
                :system-include-path '("/usr/include")
                :spp-table '() )
