#echo $1 $2 $3 $4
#Usage Progam[without .c  extention] Function1 Function2 Function3 
#CFG of specified Functions will be displayed as png on screen
./transform-slice-transfrom.sh $1
opt --dot-cfg $1.oh -o $1.oh.cfg
dot -Tpng cfg.main.dot>$1.oh.main.png
if [-z "$2"]
  then
	echo "no $2"
  else
	echo "has $2"
        dot -Tpng cfg.$2.dot>$1.oh.$2.png
        eog $1.oh.$2.png &
fi
if [ -z "$3" ]
    then
        echo "no $2"
  else
	echo "has $3"
        dot -Tpng cfg.$3.dot>$1.oh.$3.png
        eog $1.oh.$3.png &
fi
if [ -z "$4" ]
  then
        echo "no $2"
  else
	echo "has $4"
        dot -Tpng cfg.$4.dot>$1.oh.$4.png
        eog $1.oh.$4.png &
fi

#dot -Tpng cfg.$2.dot>$1.oh.$2.png
#dot -Tpng cfg.$3.dot>$1.oh.$3.png
#dot -Tpng cfg.$4.dot>$1.oh.$4.png
#eog $1.oh.main.png &
#eog $1.oh.$2.png &
#eog $1.oh.$3.png &
#eog $1.oh.$4.png &


opt --dot-cfg $1.oh.sliced
dot -Tpng cfg.main.dot>$1.oh.sliced.main.png
if [ -z "$2" ]
  then
        echo "no"
  else
        dot -Tpng cfg.$2.dot>$1.oh.sliced.$2.png
        eog $1.oh.sliced.$2.png &
fi
if [ -z "$3" ]
  then
        echo "no"
  else
        dot -Tpng cfg.$3.dot>$1.oh.sliced.$3.png
        eog $1.oh.sliced.$3.png &
fi
if [ -z "$4" ]
  then
        echo "no"
  else
        dot -Tpng cfg.$4.dot>$1.oh.sliced.$4.png
        eog $1.oh.sliced.$4.png &
fi

#dot -Tpng cfg.$2.dot>$1.oh.sliced.$2.png
#dot -Tpng cfg.$3.dot>$1.oh.sliced.$3.png
#dot -Tpng cfg.$4.dot>$1.oh.sliced.$4.png
#eog $1.oh.sliced.main.png &
#eog $1.oh.sliced.$2.png &
#eog $1.oh.sliced.$3.png &
#eog $1.oh.sliced.$4.png &


opt --dot-cfg $1.verifier
dot -Tpng cfg.main.dot>$1.verifier.main.png
if [ -z "$2" ]
  then
        echo "no"
  else
        dot -Tpng cfg.$2.dot>$1.verifier.$2.png
        eog $1.verifier.$2.png &
fi
if [ -z "$3" ]
  then
        echo "no"
  else
        dot -Tpng cfg.$3.dot>$1.verifier.$3.png
        eog $1.verifier.$3.png &
fi
if [ -z "$4" ]
  then
        echo "no"
  else
        dot -Tpng cfg.$4.dot>$1.verifier.$4.png
        eog $1.verifier.$4.png &
fi

#dot -Tpng cfg.$2.dot>$1.verifier.$2.png
#dot -Tpng cfg.$3.dot>$1.verifier.$3.png
#dot -Tpng cfg.$4.dot>$1.verifier.$4.png
#eog $1.verifier.main.png &
 
