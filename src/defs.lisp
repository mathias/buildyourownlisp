(def {fun} (\ {args body} {def (head args) (\ (tail args) body)}))

(fun {unpack f xs} {eval (join (list f) xs)})
(fun {pack f & xs} {f xs})

(def {uncurry} pack)
(def {curry} unpack)

(fun {first lst} {eval (head lst)})
(fun {second lst} {eval (head (tail lst))})

(fun {len l} { if (== l {}) {0} {+ 1 (len (tail l))} })
(fun {reverse l} { if (== l {}) {{}} {join (reverse (tail l)) (head l)} })

(fun {nth lst n} { if (== n 0) {eval (head lst)} {nth (tail lst) (- n 1)}})
