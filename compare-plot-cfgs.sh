#dump $1 and $3,$4 functions
opt --dot-cfg $1
dot -Tpng cfg.main.dot>$1.main.png
if [ -z "$3" ]
    then
        echo "no $3"
  else
        echo "has $3"
        dot -Tpng cfg.$3.dot>$1.$3.png
        eog $1.$3.png &
fi
if [ -z "$4" ]
  then
        echo "no $4"
  else
        echo "has $4"
        dot -Tpng cfg.$4.dot>$1.$4.png
        eog $1.$4.png &
fi
#dump $2 and $3,$4 functions
opt --dot-cfg $2
dot -Tpng cfg.main.dot>$2.main.png
if [ -z "$3" ]
    then
        echo "no $3"
  else
        echo "has $3"
        dot -Tpng cfg.$3.dot>$2.$3.png
        eog $2.$3.png &
fi
if [ -z "$4" ]
  then
        echo "no $4"
  else
        echo "has $4"
        dot -Tpng cfg.$4.dot>$2.$4.png
        eog $2.$4.png &
fi



       
