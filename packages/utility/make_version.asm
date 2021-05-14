; see https://board.flatassembler.net/topic.php?p=191135
t = __TIME__/50

virtual at 0 ; capitals to differentiate from orgin
BASE36::db '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ'
assert $ = 36
end virtual

output = 0
repeat 5
	char = t mod 36
	t = t/36
	load char:1 from BASE36:char
	output = output or (char shl ((%%-%) shl 3))
end repeat
; see https://board.flatassembler.net/topic.php?p=213814
output = output and $FFFF'FFFF ; remove least char
format binary as '\version.inc'
db 'VERSION equ "',string output,'"',10
