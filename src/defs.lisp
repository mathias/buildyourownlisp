(def {fun} (\ {args body} {def (head args) (\ (tail args) body)}))

(fun {unpack f xs} {eval (join (list f) xs)})
(fun {pack f & xs} {f xs})

(def {uncurry} pack)
(def {curry} unpack)

(fun {first lst} {eval (head lst)})
(fun {second lst} {eval (head (tail lst))})
