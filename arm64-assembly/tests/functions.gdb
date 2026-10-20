set confirm off
break *double_plus
run
set $entry_sp = $sp
set $entry_x19 = $x19
stepi 4
nexti
stepi 3
if $sp != $entry_sp
    quit 1
end
if $x19 != $entry_x19
    quit 1
end
kill
quit 0
