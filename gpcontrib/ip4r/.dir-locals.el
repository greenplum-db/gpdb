;; largely stolen from pg itself

((c-mode . ((c-basic-offset . 4)
            (c-file-style . "bsd")
	    (c-file-offsets (case-label . +) (label . -) (statement-case-open . +))
            (fill-column . 78)
            (indent-tabs-mode . t)
            (tab-width . 4)
	    (eval add-hook 'before-save-hook 'delete-trailing-whitespace nil t)))
 (css-mode . ((tab-width . 4)
	      (eval add-hook 'before-save-hook 'delete-trailing-whitespace nil t)))
 (dsssl-mode . ((indent-tabs-mode . nil)))
 (nxml-mode . ((indent-tabs-mode . nil))))

;; c-file-offsets is not marked safe by default, but you can either
;; accept the specific value given as safe always, or do something
;; like this in your .emacs to accept only the simplest offset lists
;; automatically:
;; (defun my-safe-c-file-offsets-p (alist)
;;  (catch 'break
;;    (and (listp alist)
;;         (dolist (elt alist t)
;;           (pcase elt
;;             (`(,(pred symbolp) . ,(or `+ `- `++ `-- `* `/)) t)
;;             (`(,(pred symbolp) . ,(or (pred null) (pred integerp))) t)
;;             (`(,(pred symbolp) . [ ,(pred integerp) ]) t)
;;             (_ (throw 'break nil)))))))
;; (put 'c-file-offsets 'safe-local-variable 'my-safe-c-file-offsets-p)
