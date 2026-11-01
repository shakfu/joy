# Joy Library Reference

Auto-generated documentation from Joy library files.

## Contents

- [agglib.joy](#agglibjoy)
- [alljoy.joy](#alljoyjoy)
- [fib.joy](#fibjoy)
- [flatjoy.joy](#flatjoyjoy)
- [fraclib.joy](#fraclibjoy)
- [gcd.joy](#gcdjoy)
- [grmlib.joy](#grmlibjoy)
- [grmtst.joy](#grmtstjoy)
- [inilib.joy](#inilibjoy)
- [joytut.joy](#joytutjoy)
- [jp-church.joy](#jp-churchjoy)
- [jp-joyjoy.joy](#jp-joyjoyjoy)
- [jp-joytst.joy](#jp-joytstjoy)
- [jp-nestrec.joy](#jp-nestrecjoy)
- [jp-reprodtst.joy](#jp-reprodtstjoy)
- [lazlib.joy](#lazlibjoy)
- [laztst.joy](#laztstjoy)
- [lsplib.joy](#lsplibjoy)
- [lsptst.joy](#lsptstjoy)
- [mandel.joy](#mandeljoy)
- [mandel1.joy](#mandel1joy)
- [mandel2.joy](#mandel2joy)
- [mapreduce.joy](#mapreducejoy)
- [modtst.joy](#modtstjoy)
- [mthlib.joy](#mthlibjoy)
- [mtrlib.joy](#mtrlibjoy)
- [mtrtst.joy](#mtrtstjoy)
- [numlib.joy](#numlibjoy)
- [plglib.joy](#plglibjoy)
- [plgtst.joy](#plgtstjoy)
- [prelib.joy](#prelibjoy)
- [quad.joy](#quadjoy)
- [quadratic.joy](#quadraticjoy)
- [replib.joy](#replibjoy)
- [reptst.joy](#reptstjoy)
- [seqlib.joy](#seqlibjoy)
- [symlib.joy](#symlibjoy)
- [symtst.joy](#symtstjoy)
- [test.joy](#testjoy)
- [tutinp.joy](#tutinpjoy)
- [tutlib.joy](#tutlibjoy)
- [typlib.joy](#typlibjoy)
- [usrlib.joy](#usrlibjoy)

## agglib.joy

### Operators

#### `unitset`

```joy
unitset == {} cons
```


#### `unitstring`

```joy
unitstring == "" cons
```


#### `unitlist`

```joy
unitlist == [] cons
```


#### `pairset`

```joy
pairset == {} cons cons
```


#### `pairstring`

```joy
pairstring == "" cons cons
```


#### `pairlist`

```joy
pairlist == [] cons cons
```


#### `unpair`

```joy
unpair == uncons uncons pop
```


#### `second`

```joy
second == rest first
```


#### `third`

```joy
third == rest rest first
```


#### `fourth`

```joy
fourth == 3 drop first
```


#### `fifth`

```joy
fifth == 4 drop first
```


#### `string2set`

```joy
string2set == {} swap shunt
```


#### `elements`

```joy
elements == {} swap [swons] step
```


#### `set2string`

```joy
set2string == "" [[chr] dip cons] foldr
```


#### `shunt`

```joy
shunt == [swons] step
```


#### `nulld`

```joy
nulld == [null] dip
```

"dipped" versions


#### `consd`

```joy
consd == [cons] dip
```


#### `swonsd`

```joy
swonsd == [swons] dip
```


#### `unconsd`

```joy
unconsd == [uncons] dip
```


#### `unswonsd`

```joy
unswonsd == [unswons] dip
```


#### `firstd`

```joy
firstd == [first] dip
```


#### `restd`

```joy
restd == [rest] dip
```


#### `secondd`

```joy
secondd == [second] dip
```

R.W.


#### `thirdd`

```joy
thirdd == [third] dip
```


### On Two Operands

#### `null2`

```joy
null2 == nulld null or
```

on two operands


#### `cons2`

```joy
cons2 == swapd cons consd
```


#### `uncons2`

```joy
uncons2 == unconsd uncons swapd
```


#### `swons2`

```joy
swons2 == swapd swons swonsd
```


#### `unswons2`

```joy
unswons2 == [unswons] dip unswons swapd
```


#### `zip`

```joy
zip == [null2] [pop2 []] [uncons2] [[pairlist] dip cons] linrec
```


#### `from-to`

```joy
from-to ==
    [] cons  [pop pop] swoncat
	[>] swap
	[ [dup succ] dip ]
	[cons]
	linrec
```


#### `from-to-list`

```joy
from-to-list == [] from-to
```


#### `from-to-set`

```joy
from-to-set == {} from-to
```


#### `from-to-string`

```joy
from-to-string == "" from-to
```


### Combinators

#### `pairstep`

```joy
pairstep == [dupd] swoncat [step pop] cons cons step
```

cartesian product -like


#### `mapr`

```joy
mapr == [ [null] [] [uncons] ] dip
	[dip cons] cons
	linrec
```

Right to Left


#### `foldr`

```joy
foldr ==
    [ [ [null] ] dip
	  [] cons [pop] swoncat
	  [uncons] ] dip
	linrec
```


#### `stepr2`

```joy
stepr2 ==
    [ [null2]  [pop pop] ] dip
	[dip] cons [dip] cons [uncons2] swoncat
	tailrec
```


#### `fold2`

```joy
fold2 == rollupd stepr2
```


#### `mapr2`

```joy
mapr2 == [ [null2] [pop2 []] [uncons2] ] dip
	[dip cons] cons
	linrec
```


#### `foldr2`

```joy
foldr2 ==
    [ [ [null2] ] dip
	  [] cons [pop2] swoncat
	  [uncons2] ] dip
	linrec
```


#### `interleave2`

```joy
interleave2 == [cons cons] foldr2
```


#### `interleave2list`

```joy
interleave2list == [] interleave2
```


#### `sum`

```joy
sum == 0 [+] fold
```


#### `average`

```joy
average == [sum] [size] cleave /
```


#### `variance`

```joy
variance ==
    0.0 swap dup
	[sum] [size] cleave dup
	[ /
	  [ - dup * + ] cons
	  step ]
	dip
	pred /
```


#### `AGGLIB`

```joy
AGGLIB == "agglib.joy - aggregate library\n"
```


---

## alljoy.joy

---

## fib.joy

---

## flatjoy.joy

### Definitions

#### `P0`

```joy
P0 == []
```


#### `P1`

```joy
P1 == [2 3 + dup *]
```


#### `P2`

```joy
P2 == [[1 2 3] [dup *] map]
```


#### `P3`

```joy
P3 == [ [[1 2][3 4][5] []] [[dup *] map] map ]
```


#### `c`

```joy
c == concat
```


#### `j2f-f`

```joy
j2f-f ==
    [ list ]
        [ [[[]] concat] dip [j2f-f] step [[] cons c] concat ]
        [ [] cons [c] cons concat ]
        ifte
```


#### `j2f-forwards`

```joy
j2f-forwards == [[]] swap [j2f-f] step
```


#### `s`

```joy
s == swoncat
```


#### `j2f-r`

```joy
j2f-r ==
    [ list ]
        [ [[swons] swoncat] dip [j2f-r] step [] swons ]
        [ [] cons [s] cons swoncat ]
        ifte
```


#### `j2f-reverse`

```joy
j2f-reverse == [] swap [j2f-r] step [] swons
```


---

## fraclib.joy

### Definitions

#### `nrows`

```joy
nrows == 30
```


#### `ncols`

```joy
ncols == 75
```

size of output display


#### `mandel`

```joy
mandel ==
    -1 nrows ncols *
  	    [ succ dup dup [col] dip row
              get_c1 swap get_c2
              get_k putpt
              [eol] ['\n putch] [] ifte
            ] times
```

size of output display


#### `row`

```joy
row == ncols /
```

loop for each cell


#### `col`

```joy
col == ncols rem
```

I:i -> I:row


#### `eol`

```joy
eol == col ncols pred =
```

I:i -> I:col


#### `putpt`

```joy
putpt == 16 rem " .:,;!/>)|&IH%*#" of putch
```

I:i -> B


#### `get_k`

```joy
get_k ==
    pairlist [0 0] swoncat
            0 [112 <= [eoloop] dip and]
            [succ [inc_zs] dip] while
            popd 1 -
```

we've a palette of 16


#### `get_c1`

```joy
get_c1 == 0.10 * 1.5 -
```

cleanup               -> I


#### `get_c2`

```joy
get_c2 == 0.04 * 2.0 -
```

outer loop coeff. I:i -> F


#### `sqr_z1`

```joy
sqr_z1 == first dup *
```

inner loop coeff. I:i -> F


#### `sqr_z2`

```joy
sqr_z2 == 1 at  dup *
```

A -> I


#### `inc_z1`

```joy
inc_z1 == dup [sqr_z1] [sqr_z2] cleave -
            [3 at] dip +
```

A -> I


#### `inc_z2`

```joy
inc_z2 ==
    3 take reverse uncons uncons
            uncons pop
            2 * * +
```

A -> I


#### `eoloop`

```joy
eoloop == dup sqr_z1 swap sqr_z2 + 10 <=
```

A -> I


#### `inc_zs`

```joy
inc_zs ==
    dup [inc_z1] [inc_z2] cleave
            pairlist swap rest rest concat
```

(z1*z1)+(z2*z2)<=10 A -> B


#### `FRACLIB`

```joy
FRACLIB == "fraclib.joy - fractal library\n"
```

A -> A


---

## gcd.joy

---

## grmlib.joy

### Definitions

#### `unops`

```joy
unops == [ ? * +  $ ]
```

define the operators used by regular expressions and grammars:


#### `binops`

```joy
binops == [ | _ ]
```


#### `bin1ops`

```joy
bin1ops == [ | ]
```


#### `bin2ops`

```joy
bin2ops == [ _ ]
```


#### `bin3ops`

```joy
bin3ops == [ ]
```


### Generating

#### `show-put`

```joy
show-put == pop put newline pop
```

the various initial continuations:


#### `show-put-step`

```joy
show-put-step == pop [put space] step newline pop
```


#### `show-putchars-step`

```joy
show-putchars-step == pop [putchars] step newline pop
```


#### `show-putchars-sp-step`

```joy
show-putchars-sp-step == pop [putchars space] step newline pop
```


#### `accumulate`

```joy
accumulate ==
    pop popd swons
IN
    generate ==
      [ pop pop size <= ]
      [ pop pop pop pop ]
      [
	"generate B" gen-trace
	unswons
	[ [ [[ | ] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip generate] dip ]
	    dip
	    [i] dip generate ]
	  [ [[ _ ] in ]
	    pop uncons first
	    [ [generate] cons swons ]
	    dip generate ]
	  [ [[ ? ] in ]
	    pop
	    [ [] cons cons cons dup
	      [i unswons i] dip ]
	    dip
	    [i] dip first generate ]
	  [ [[ + ] in ]
	    swons
	    [] cons cons cons cons dup
	    [ i second generate ] dip
	    i dup
	    [ [generate] cons swons ] dip
	    second generate ]
	  [ [[ * ] in ]
	    swons
	    [ [] cons cons cons dup
	      [i unswons i] dip ]
	    dip
	    dup
	    [ [i] dip [generate] cons swons ] dip
	    second generate ]
	  [ [[ $ ] in]
	    pop first i
	    Min2Tre generate ]
	  [ [[QUOTE] in ]
	    pop first
	    swap [swons] dip
	    unswons i ]
	  [ [[epsilon] in ]
	    pop pop
	    unswons i ]
	  [
	    popd
	    swap [swons] dip
	    unswons i ] ]
	cond
	"generate E" gen-trace
	]
      ifte
```


#### `generate`

```joy
generate ==
    [ pop pop size <= ]
      [ pop pop pop pop ]
      [
	"generate B" gen-trace
	unswons
	[ [ [[ | ] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip generate] dip ]
	    dip
	    [i] dip generate ]
	  [ [[ _ ] in ]
	    pop uncons first
	    [ [generate] cons swons ]
	    dip generate ]
	  [ [[ ? ] in ]
	    pop
	    [ [] cons cons cons dup
	      [i unswons i] dip ]
	    dip
	    [i] dip first generate ]
	  [ [[ + ] in ]
	    swons
	    [] cons cons cons cons dup
	    [ i second generate ] dip
	    i dup
	    [ [generate] cons swons ] dip
	    second generate ]
	  [ [[ * ] in ]
	    swons
	    [ [] cons cons cons dup
	      [i unswons i] dip ]
	    dip
	    dup
	    [ [i] dip [generate] cons swons ] dip
	    second generate ]
	  [ [[ $ ] in]
	    pop first i
	    Min2Tre generate ]
	  [ [[QUOTE] in ]
	    pop first
	    swap [swons] dip
	    unswons i ]
	  [ [[epsilon] in ]
	    pop pop
	    unswons i ]
	  [
	    popd
	    swap [swons] dip
	    unswons i ] ]
	cond
	"generate E" gen-trace
	]
      ifte
```


#### `gen-trace-default`

```joy
gen-trace-default == putchars " : " putchars stack putln
```


#### `gen-trace`

```joy
gen-trace == pop
```


### The Four Basic Wrappers

#### `gen-put`

```joy
gen-put == Min2Tre  [ [] [[show-put]] ] dip generate
```

the four basic wrappers


#### `gen-put-step`

```joy
gen-put-step == Min2Tre  [ [] [[show-put-step]] ] dip generate
```


#### `gen-putchars-step`

```joy
gen-putchars-step == Min2Tre  [ [] [[show-putchars-step]] ] dip generate
```


#### `gen-putchars-sp-step`

```joy
gen-putchars-sp-step == Min2Tre  [ [] [[show-putchars-sp-step]] ] dip generate
```


#### `gen-accumulate`

```joy
gen-accumulate == Min2Tre  [ [[]] dip [] [[accumulate]] ] dip generate
```


### Parsing

#### `parse-string-residues`

```joy
parse-string-residues == pop [ succ dup put ": " putchars ] dip [putch] step newline
```

the various initial continuations:


#### `parse-list-residues`

```joy
parse-list-residues ==
    pop [ succ dup put ": " putchars ] dip [put space] step newline
```


#### `parse-test`

```joy
parse-test == pop pop true or
```


#### `parse-count`

```joy
parse-count == pop pop succ
```


#### `parse-tell`

```joy
parse-tell ==
    "accept\n" putchars pop pop succ
IN
    parse ==
	"parse B" prs-trace
	unswons
	[ [ [[ | ] in]
	    pop
	    [ [] cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip parse] dip ]
	    dip
	    [i] dip parse ]
	  [ [[ _ ] in ]
	    pop uncons first swap
	    [ [parse] cons swons ]
	    dip parse ]
	  [ [[ ? ] in ]
	    pop
	    [ [] cons cons dup
	      [i unswons i] dip ]
	    dip
	    [i] dip first parse ]
	  [ [[ + ] in ]
	    swons
	    [] cons cons cons dup
	    [ i second parse ] dip
	    i dup
	    [ [parse] cons swons ] dip
	    second parse ]
	  [ [[ * ] in ]
	    swons
	    [ [] cons cons dup
	      [i unswons i] dip ]
	    dip
	    dup
	    [ [i] dip [parse] cons swons ] dip
	    second parse ]
	  [ [[ $ ] in]
	    pop first i
	    Min2Tre parse ]
	  [ [[QUOTE] in ]
	    pop first
	    swap [swons] dip
	    unswons i ]
	  [ [[epsilon] in ]
	    pop pop
	    unswons i ]
	  [
	    popd
	    [ [pop first] dip = ]
	    [ pop [rest] dip unswons i ]
	    [ pop pop pop ]
	    ifte ] ]
	cond
	"parse E" prs-trace
```


#### `parse`

```joy
parse ==
    "parse B" prs-trace
	unswons
	[ [ [[ | ] in]
	    pop
	    [ [] cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip parse] dip ]
	    dip
	    [i] dip parse ]
	  [ [[ _ ] in ]
	    pop uncons first swap
	    [ [parse] cons swons ]
	    dip parse ]
	  [ [[ ? ] in ]
	    pop
	    [ [] cons cons dup
	      [i unswons i] dip ]
	    dip
	    [i] dip first parse ]
	  [ [[ + ] in ]
	    swons
	    [] cons cons cons dup
	    [ i second parse ] dip
	    i dup
	    [ [parse] cons swons ] dip
	    second parse ]
	  [ [[ * ] in ]
	    swons
	    [ [] cons cons dup
	      [i unswons i] dip ]
	    dip
	    dup
	    [ [i] dip [parse] cons swons ] dip
	    second parse ]
	  [ [[ $ ] in]
	    pop first i
	    Min2Tre parse ]
	  [ [[QUOTE] in ]
	    pop first
	    swap [swons] dip
	    unswons i ]
	  [ [[epsilon] in ]
	    pop pop
	    unswons i ]
	  [
	    popd
	    [ [pop first] dip = ]
	    [ pop [rest] dip unswons i ]
	    [ pop pop pop ]
	    ifte ] ]
	cond
	"parse E" prs-trace
```


#### `prs-test`

```joy
prs-test == Min2Tre [ [false] dip [[parse-test]] ] dip parse
```


#### `prs-string-residues`

```joy
prs-string-residues == Min2Tre [ [0] dip [[parse-string-residues]] ] dip parse pop
```


#### `prs-list-residues`

```joy
prs-list-residues == Min2Tre [ [0] dip [[parse-list-residues]] ] dip parse pop
```


#### `prs-count`

```joy
prs-count == Min2Tre [ [0] dip [[parse-count]] ] dip parse
```


### Other Stuff To Go Here

#### `GRMLIB`

```joy
GRMLIB == "grmlib.joy - grammar (generating/parsing) library\n"
```

other stuff to go here


---

## grmtst.joy

### The Grammar Has Only One Production

#### `arith`

```joy
arith == [ "x"  |  "(" _ $ arith _ "+" _ $ arith _ ")" ]
```


#### `expression`

```joy
expression == [ $ term  _  * ["+" _ $ term] ]
```


#### `term`

```joy
term == [ $ factor  _  * ["*" _ $ factor] ]
```


#### `factor`

```joy
factor == [ "x"  |  "(" _ $ expression _ ")" ]
```


### Parsing

#### `prs-trace`

```joy
prs-trace == pop
```


#### `tree`

```joy
tree == [ "big" _ "tree" ]
```


#### `names`

```joy
names ==
    [ "peter" _ "smith" | "paul" _ "jones" | "mary" _
               "robinson" ]
```


#### `anyname`

```joy
anyname ==
    [ ["peter" | "paul" | "mary"] _ ["smith" | "jones" | "robinson" ] ]
```


#### `Polish`

```joy
Polish ==
    [  'p
	       |  'N _ $ Polish
	       |  ['A | 'C | 'K] _ $ Polish _ $ Polish ]
```


#### `string2charlist`

```joy
string2charlist == [null]  [pop []]  [uncons]  [cons]  linrec
```


#### `Rev-Pol`

```joy
Rev-Pol == [ 'p _ * [ 'N
			| $ Rev-Pol _ ['A | 'C | 'K] ] ]
```


#### `sentence`

```joy
sentence ==
    [ $ noun-phrase _ $ verb-phrase  _ * [["and" | "or"] _ $ sentence] ]
```


#### `noun-phrase`

```joy
noun-phrase ==
    [ "John" | "Mary" | ["a" | "the"] _ * $ adjective _ ["cat" | "dog"] ]
```


#### `verb-phrase`

```joy
verb-phrase ==
    [ "is" _ ? "not" _ $ adjective
  | "sleeps"
  | ["eats" | "sits" _ "on"] _ $ noun-phrase ]
```


#### `adjective`

```joy
adjective == [ "brown" | "black" | "rich" ]
```


#### `E`

```joy
E == sentence prs-list-residues
```


---

## inilib.joy

### Inputoutput

#### `newline`

```joy
newline == '\n putch
```


#### `putln`

```joy
putln == put newline
```


#### `space`

```joy
space == '\032 putch
```


#### `bell`

```joy
bell == '\007 putch
```


#### `putstrings`

```joy
putstrings == [putchars] step
```


#### `ask`

```joy
ask == "Please " putchars putchars newline get
```


### Operators

#### `dup2`

```joy
dup2 == dupd dup swapd
```


#### `pop2`

```joy
pop2 == pop pop
```


#### `newstack`

```joy
newstack == [] unstack
```


#### `truth`

```joy
truth == true
```


#### `falsity`

```joy
falsity == false
```


#### `to-upper`

```joy
to-upper == ['a >=] [32 -] [] ifte
```


#### `to-lower`

```joy
to-lower == ['a < ] [32 +] [] ifte
```


#### `boolean`

```joy
boolean == [logical] [set] sequor
```


#### `numerical`

```joy
numerical == [integer] [float] sequor
```


#### `swoncat`

```joy
swoncat == swap concat
```


### Date And Time

#### `weekdays`

```joy
weekdays ==
    [ "Monday" "Tuesday" "Wednesday" "Thursday" "Friday"
          "Saturday" "Sunday" ]
```

date and time


#### `months`

```joy
months ==
    [ "JAN" "FEB" "MAR" "APR" "MAY" "JUN"
	  "JUL" "AUG" "SEP" "OCT" "NOV" "DEC" ]
```


#### `localtime-strings`

```joy
localtime-strings ==
    time localtime
	[ [ 0 at 'd 4 4 format			]
	  [ 1 at pred months of			]
	  [ 2 at 'd 2 2 format			]
	  [ 3 at 'd 2 2 format			]
	  [ 4 at 'd 2 2 format			]
	  [ 5 at 'd 2 2 format			]
	  [ 6 at [] ["true"] ["false"] ifte	]
	  [ 7 at 'd 5 5 format			]
	  [ 8 at pred weekdays of		] ]
	[i] map
	popd
```


#### `today`

```joy
today ==
    localtime-strings
	[ [8 at] [" "] [2 at] ["-"] [1 at] ["-"] [0 at rest rest] ]
	[i] map
	popd
	"" [concat] fold
```


#### `now`

```joy
now ==
    localtime-strings
	3 drop
	[ [0 at] [":"] [1 at] [":"] [2 at] ]
	[i] map
	popd
	"" [concat] fold
```


#### `show-todaynow`

```joy
show-todaynow == today putchars space now putchars newline
```


### Program Operators

#### `conjoin`

```joy
conjoin == [[false] ifte] cons cons
```

program operators


#### `disjoin`

```joy
disjoin == [ifte] cons [true] swons cons
```


#### `negate`

```joy
negate == [[false] [true] ifte] cons
```


### Combinators

#### `sequor`

```joy
sequor == [pop true] swap ifte
```


#### `sequand`

```joy
sequand == [pop false] ifte
```


#### `dipd`

```joy
dipd == [dip] cons dip
```


#### `dip2`

```joy
dip2 == [dip] cons dip
```


#### `dip3`

```joy
dip3 == [dip2] cons dip
```


#### `call`

```joy
call == [] cons i
```


#### `i2`

```joy
i2 == [dip] dip i
```


#### `nullary2`

```joy
nullary2 == [nullary] cons dup i2 swapd
```


#### `repeat`

```joy
repeat == dupd swap [i] dip2 while
```


#### `forever`

```joy
forever == maxint swap times
```


### Library Inclusion

#### `verbose`

```joy
verbose == false
```

library inclusion


#### `libload`

```joy
libload ==
    [ '_ swons intern body null ]
	[ ".joy" concat include ]
	[ [ verbose ]
	  [ putchars "  is already loaded\n" putchars ]
	  [ pop ]
	  ifte ]
	ifte
```


#### `basic-libload`

```joy
basic-libload == "agglib" libload
	"seqlib" libload
	"numlib" libload
```


#### `special-libload`

```joy
special-libload ==
    "mtrlib" libload
	"tutlib" libload
	"lazlib" libload
	"lsplib" libload
	"symlib" libload
```


#### `all-libload`

```joy
all-libload == basic-libload special-libload
```


#### `INILIB`

```joy
INILIB == "inilib.joy - the initial library, assumed everywhere\n"
```


---

## joytut.joy

### Definitions

#### `toc-of-tutorial`

```joy
toc-of-tutorial ==
    [ [ [sec0] "Interactive tutorial on JOY"	]
	  [ [sec1] "1 - Numbers and Lists"		]
	  [ [sec2] "2 - The map and filter combinators"	]
	  [ [sec3] "3 - The ifte and linrec combinators"] ]
```


#### `q1`

```joy
q1 ==
    "What will the program   " putchars
	putchars
	"   leave on the stack ?\n" putchars
	i
	expecting
```


#### `q2`

```joy
q2 ==
    "What will the dots   ...   have to be, so that\n\t" putchars
	putchars
	"\nleaves    " putchars
	putchars
	"    on the stack ?\n" putchars
	i
	expecting
```


#### `sec0`

```joy
sec0 == toc-of-tutorial rest [first i] step
```

main sections:


#### `sec1`

```joy
sec1 ==
    1 toc-one-heading
ini-stats
[ "One of the data types in Joy is the type of integers.\n"
  "Literal integers are written in ordinary decimal notation.\n"
  "The usual binary operators are:  +  -  *  /\n"
  "But these operators are written after their arguments.\n" ]
putstrings
[get-integer  5 ]	"2  3  +"			q1
[get-integer 42 ]	"7  6  *"			q1
[get-integer 37 ]	"7  6  *  2  3  +  -"		q1
[ "The most important aggregate datatype is that of lists.\n"
  "These are written inside square brackets, like this:\n"
  "        [42 17 3 9 35]\n"
  "Some important operators for lists are:\n"
  "    first      rest      cons      concat\n" ]
putstrings
[get-integer 5           ]  "[5 4 2 7]  first"		q1
[get-list  [4 2 7]       ]  "[5 4 2 7]  rest"		q1
[get-list  [8 9 3]       ]  "8  [9 3]  cons"		q1
[get-list  [5 3 5 3]     ]  "[5 3]  dup  concat"	q1
[get-list  [[5 3] 5 3]   ]  "[5 3]  dup  cons"		q1
[get-list  [7 3]	 ]  "[7 3 3 2 4]"
			     "...  [3 2 4]  concat"	q2
put-stats
```


#### `sec2`

```joy
sec2 ==
    2 toc-one-heading
ini-stats
[ "Combinators are similar to operators, they expect something\n"
  "on the stack. But combinators always expect a quoted program\n"
  "and perhaps something else. For example, the   map   combinator\n"
  "expects a single program and below that an aggregate, e.g. a list.\n"
] putstrings
[get-list  [6 4 8]      ]  "[3 2 4] [dup +] map"	q1
[get-quote [10 + ]      ]
  "[13 12 14]"		    "[3 2 4]  ...  map"		q2
[ "Another example is the   filter   combinator, which expects a\n"
  "program that computes a truth value, and below that an aggregate.\n"
] putstrings
[get-list  [5 3 7]	 ]  "\n\t[5 16 3 7 14]  [10 <]  filter"	q1
"For the next question, provide the shortest answer\n" putchars
[get-quote [20 <]	 ]
  "[15 19 12]"	     "[5 10 9 11 2]  [10 +]  map   ...  filter"	q2
put-stats
```


#### `sec3`

```joy
sec3 ==
    3 toc-one-heading
ini-stats
[ "Two more complicated combinators are the  ifte  and the  linrec\n"
  "combinators.  The  ifte  combinator expects three quotation "
  "parameters\non the stack. In most cases they have been pushed just"
  " beforehand.\n" ] putstrings
[get-integer 15 ]  "5  [7 <]  [10 +]  [10 -]  ifte"	q1
[get-integer -2 ]  "8  [7 <]  [10 +]  [10 -]  ifte"	q1
[ "A programmer wants to write a recursive definition of the\n"
  "factorial function, which has the form:\n"
  "    factorial ==\n"
  "      ...                           (* first  quote *)\n"
  "      ...                           (* second quote *)\n"
  "      ...                           (* third  quote *)\n"
  "      ifte\n" ]
putstrings
"What could be used as the first quote?\n" putchars
get-quote [[null][0 =][1 <]]  expecting-from
"What could be used as the second quote?\n" putchars
get-quote [[pop 1][succ][1 +]]  expecting-from
"This question is harder:\n" putchars
"What could be used as the third quote?\n" putchars
get-quote [[dup pred factorial *][dup 1 - factorial *]] expecting-from
[ "Later the programmer realises that the factorial function\n"
  "is only needed in one place of the program, so it would be good\nnot"
  "to have the definition at all, by using the  linrec  combinator:\n"
  "      [ null ] 			(* first  quote *)\n"
  "      [ succ ]                       (* second quote *)\n"
  "      ...                            (* third  quote *)\n"
  "      ...                            (* fourth quote *)\n"
  "      linrec\n" ]
putstrings
"What could be used as the third quote ?\n" putchars
get-quote [[dup pred][dup 1 -]] expecting-from
"What could be used as the fourth quote ?\n" putchars
get-quote [*] expecting
put-stats
```


#### `joytut`

```joy
joytut == toc-do-sec0-loop
```


#### `JOYTUT`

```joy
JOYTUT == "joytut.joy - " toc-of-tutorial first second concat
```


---

## jp-church.joy

### Definitions

#### `C0`

```joy
C0 == pop
```


#### `Csucc`

```joy
Csucc == [dup [i] dip] dip i
```


#### `Peano`

```joy
Peano == 0 [succ]
```


#### `C1`

```joy
C1 == [C0] Csucc
```


#### `C2`

```joy
C2 == [C1] Csucc
```


#### `C3`

```joy
C3 == [C2] Csucc
```


#### `Doublings`

```joy
Doublings == 1 [2 *]
```


#### `Lists`

```joy
Lists == [] [[i] swoncat]
```


#### `Cadd`

```joy
Cadd == [ [[Csucc] cons] ] dip i i
```


#### `Cmul`

```joy
Cmul == [ [[C0]] dip  [Cadd] cons [cons] cons ] dip i i
```


#### `Cpow`

```joy
Cpow == [ [[C1 ]] dip  [Cmul] cons [cons] cons ] dip i i
```


#### `Q0`

```joy
Q0 == [pop]
```


#### `Qsucc`

```joy
Qsucc == [ [dup [i] dip] dip i ]  cons
```


#### `Q1`

```joy
Q1 == Q0 Qsucc
```


#### `Q2`

```joy
Q2 == Q1 Qsucc
```


#### `Q3`

```joy
Q3 == Q2 Qsucc
```


#### `Qadd`

```joy
Qadd == [ [[Qsucc i] cons] swap i i ]  cons cons
```


#### `Qmul`

```joy
Qmul ==
    [ [[[Q0 i]] dip [Qadd i] cons [cons] cons] dip i i ] cons cons
```


#### `Qpow`

```joy
Qpow ==
    [ [[[Q1 i]] dip [Qmul i] cons [cons] cons] dip i i ] cons cons
```


#### `Ctrue`

```joy
Ctrue == pop i
```


#### `Cfalse`

```joy
Cfalse == popd i
```


#### `Boole`

```joy
Boole == ["Yes, yes"] ["No, no"]
```


#### `Comparison`

```joy
Comparison == 2 3 [<] [>]
```


#### `Cnot`

```joy
Cnot == swapd i
```


#### `Cor`

```joy
Cor == [[Ctrue]] dipd i
```


#### `Cand`

```joy
Cand == [[Cfalse]] dip i
```


#### `Ceq0`

```joy
Ceq0 == [ [Ctrue] [pop [Cfalse]] ] dip i i
```


#### `Cifte`

```joy
Cifte == rolldown i
```


#### `Ccons`

```joy
Ccons == [i]
```


#### `Ccar`

```joy
Ccar == [Ctrue] swap i
```


#### `Ccdr`

```joy
Ccdr == [Cfalse] swap i
```


#### `Csucc`

```joy
Csucc == [dup dip] dip i
```


#### `Cadd`

```joy
Cadd == [sip] dip i
```


#### `sip`

```joy
sip == dupd dip
```


#### `Cmul`

```joy
Cmul == [cons] dip i
```


#### `Cpow`

```joy
Cpow == [[cons] cons] dip i i
```


---

## jp-joyjoy.joy

### Definitions

#### `putchars`

```joy
putchars == [putch] step
```


#### `cr1`

```joy
cr1 == pop  [joy] cons
```


#### `cr2`

```joy
cr2 == pop [[joy] cons] app2
```


#### `cr3`

```joy
cr3 == pop [[joy] cons] app3
```


#### `cr4`

```joy
cr4 ==
    pop [[joy] cons] app4
IN
  joy  ==
    [ [ [ joy		body	joy	]
	[ true				]
	[ 'A				]
	[ []				]
	[ ""				]
	[ {}				]
	[ 0				]
	[ dup		pop	dup	]
	[ swap		pop	swap	]
	[ pop		pop	pop	]
	[ +		pop	+	]
	[ -		pop	-	]
	[ and		pop	and	]
	[ cons		pop	cons	]
	[ i		pop	joy	]
	[ dip		cr1	dip	]
	[ step		cr1	step	]
	[ map		cr1	map	]
	[ filter	cr1	filter	]
	[ times		cr1	times	]
        [ app1          cr1     app1    ]
        [ app2          cr1     app2    ]
        [ app3          cr1     app3    ]
        [ app4          cr1     app4    ]
	[ ifte		cr3	ifte	]
	[ linrec	cr4	linrec	]
	[ binrec	cr4	binrec	]
	[	      dup put [] cons i ] ]
      opcase
      i ]
    step
```


#### `joy`

```joy
joy ==
    [ [ [ joy		body	joy	]
	[ true				]
	[ 'A				]
	[ []				]
	[ ""				]
	[ {}				]
	[ 0				]
	[ dup		pop	dup	]
	[ swap		pop	swap	]
	[ pop		pop	pop	]
	[ +		pop	+	]
	[ -		pop	-	]
	[ and		pop	and	]
	[ cons		pop	cons	]
	[ i		pop	joy	]
	[ dip		cr1	dip	]
	[ step		cr1	step	]
	[ map		cr1	map	]
	[ filter	cr1	filter	]
	[ times		cr1	times	]
        [ app1          cr1     app1    ]
        [ app2          cr1     app2    ]
        [ app3          cr1     app3    ]
        [ app4          cr1     app4    ]
	[ ifte		cr3	ifte	]
	[ linrec	cr4	linrec	]
	[ binrec	cr4	binrec	]
	[	      dup put [] cons i ] ]
      opcase
      i ]
    step
```


#### `joy0`

```joy
joy0 ==
    [ [ [ joy0		body		joy0	 ]
	[ []				 	 ]
	[ pop		pop		pop	 ]
	[ cons		pop		cons	 ]
	[ opcase	pop		opcase	 ]
	[ body		pop		body	 ]
	[ i		pop		joy0 	 ]
	[ step		pop [joy0] cons	step 	 ]
	[		       [] cons	i	 ] ]
      opcase
      i ]
    step
```


#### `joy0s`

```joy
joy0s ==
    newline "joy0s :" putchars newline
    [ [ [ joy0s		body		joy0s	 ]
	[ []				 	 ]
	[ pop		pop		pop	 ]
	[ cons		pop		cons	 ]
	[ opcase	pop		opcase	 ]
	[ body		pop		body	 ]
	[ i		pop		joy0s 	 ]
	[ step		pop [joy0] cons	step 	 ]
	[		       [] cons	i	 ] ]
      opcase
      i ]
    step
```


#### `joy0l`

```joy
joy0l ==
    newline "joy0l :" putchars newline
    [ dup put space
      [ [ joy0l		body		joy0l	 ]
	[ []				 	 ]
	[ pop		pop		pop	 ]
	[ cons		pop		cons	 ]
	[ opcase	pop		opcase	 ]
	[ body		pop		body	 ]
	[ i		pop		joy0l 	 ]
	[ step		pop [joy0] cons	step 	 ]
	[		       [] cons	i	 ] ]
      opcase
      i ]
    step
```


---

## jp-joytst.joy

---

## jp-nestrec.joy

### Definitions

#### `r-fact`

```joy
r-fact == [ null ]
	[ pop 1]
	[ dup pred r-fact *]
	ifte
```


#### `r-mcc91`

```joy
r-mcc91 == [ 100 > ]
	[ 10 - ]
	[ 11 + r-mcc91 r-mcc91 ]
	ifte
```


#### `r-ack`

```joy
r-ack ==
    [ [ [pop null]  popd succ ]
	  [ [null]  pop pred 1 r-ack ]
	  [ [dup pred swap] dip pred r-ack r-ack ] ]
	cond
```


#### `r-hamilhyp`

```joy
r-hamilhyp ==
    [ null ]
	[ pop ]
	[ dup rollup pred       r-hamilhyp
	  dupd cons swap pred   r-hamilhyp ]
	ifte
```


#### `x-fact`

```joy
x-fact ==
    [ [ pop null ]
	  [ pop pop 1]
	  [ [dup pred] dip x *]
	  ifte ]
	x
```


#### `twice-x`

```joy
twice-x == dup [x] dip x
```


#### `x-mcc91`

```joy
x-mcc91 ==
    [ [ pop 100 > ]
	  [ pop 10 - ]
	  [ [11 +] dip twice-x ]
	  ifte ]
	x
```


#### `x-ack`

```joy
x-ack ==
    [ [ [ [pop pop null]  pop popd succ ]
	    [ [pop null]  [pop pred 1] dip x ]
	    [ [[dup pred swap] dip pred] dip twice-x ] ]
	cond ]
	x
```


#### `y`

```joy
y == [dup cons] swoncat dup cons i
```


#### `twice-i`

```joy
twice-i == dup [i] dip i
```


#### `y-ack`

```joy
y-ack ==
    [ [ [ [pop pop null]  pop popd succ ]
	    [ [pop null]  [pop pred 1] dip i ]
	    [ [[dup pred swap] dip pred] dip twice-i ] ]
	cond ]
	y
```


#### `x-hamilhyp`

```joy
x-hamilhyp ==
    [ [ pop null ]
	  [ pop pop ]
	  [ dup [ [dup rollup pred] dip  x ] dip
	    [dupd cons] dip
	    [swap pred] dip  x ]
	  ifte ]
	x
```


#### `ack`

```joy
ack ==
    [ [ [0 =] [pop 1 +] ]
      [ [swap 0 =] [popd 1 - 1 swap] [] ]
      [ [dup rollup [1 -] dip] [swap 1 - ack] ] ]
    condlinrec
```


#### `l-mcc91`

```joy
l-mcc91 == [ 100 > ]
	[ 10 - ]
	[ 11 + ]
	[ l-mcc91 ]
	linrec
```


#### `clr-ack`

```joy
clr-ack ==
    [ [ [pop null]  [popd succ] ]
	  [ [null]  [pop pred 1]  [] ]
	  [ [[dup pred swap] dip pred]  [clr-ack] ] ]
	condlinrec
```


#### `lr-hamilhyp`

```joy
lr-hamilhyp ==
    [ null ]
	[ pop ]
	[ dup rollup pred ]
	[ dupd cons swap pred lr-hamilhyp ]
	linrec
```


#### `toggle`

```joy
toggle == [has]  [[not] dip swons not]  [swons]  ifte
```


#### `lr-grayseq`

```joy
lr-grayseq ==
    [ null ]
	[ pop ]
	[ dup rollup pred ]
	[ dupd
	  dup [first swap toggle] dip
	  cons swap pred lr-grayseq ]
	linrec
```


#### `cnr-hamilhyp`

```joy
cnr-hamilhyp ==
    [ [ [null] [pop] ]
	  [ [dup rollup pred]
	    [dupd cons swap pred]
	    [] ] ]
	condnestrec
```


#### `cnr-ack`

```joy
cnr-ack ==
    [ [ [pop null]  [popd succ] ]
	  [ [null]  [pop pred 1]  [] ]
	  [ [[dup pred swap] dip pred]  []  [] ] ]
	condnestrec
```


#### `cnr-grayseq`

```joy
cnr-grayseq ==
    [ [ [null]  [pop] ]
	  [ [dup rollup pred]
	    [dupd
	     dup [first swap toggle] dip
	     cons swap pred]
	    [] ] ]
	condnestrec
```


#### `cnr-hanoi`

```joy
cnr-hanoi ==
    [[rolldown] infra] dip
	[ [ [null] [pop pop] ]
	  [ [dup2 [[rotate] infra] dip pred]
	    [ [dup rest put] dip
	      [[swap] infra] dip pred ]
	    [] ] ]
	condnestrec
```


#### `cnr-fact`

```joy
cnr-fact == [ [ [null] [pop 1] ]
	  [ [dup pred] [*] ] ]
	condnestrec
```


#### `cnr-mcc91`

```joy
cnr-mcc91 == [ [ [100 >] [10 -] ]
	  [ [11 +] [] [] ] ]
	condnestrec
```


#### `cnr-even`

```joy
cnr-even ==
    [ [ [2 rem null] [pop true] ]
	  [ [pop false] ] ]
	condnestrec
```


#### `cnr-abs`

```joy
cnr-abs == [ [ [0 <] [0 swap -] ]
	  [ [] ] ]
	condnestrec
```


---

## jp-reprodtst.joy

### Definitions

#### `str-maker`

```joy
str-maker ==
    dupd dup [i] dip
        [str-maker] cons cons
        [] cons cons
```


#### `str-first`

```joy
str-first == first
```


#### `str-rest`

```joy
str-rest == second i
```


#### `str-ints`

```joy
str-ints == 0 [succ] str-maker
```


#### `str-pows`

```joy
str-pows == 1 [2 *] str-maker
```


#### `str-prim`

```joy
str-prim == 2 [succ [prime not] [succ] while] str-maker
```


#### `str-brak`

```joy
str-brak == [] [[] cons] str-maker
```


#### `str-uncons`

```joy
str-uncons == [str-first] [str-rest]  cleave
```


#### `str-third`

```joy
str-third == str-rest str-rest str-rest str-first
```


#### `str-tenth`

```joy
str-tenth == 10 [str-rest] times str-first
```


#### `str-drop`

```joy
str-drop == [str-rest] times
```


#### `str-n-th`

```joy
str-n-th == str-drop str-first
```


#### `str-take`

```joy
str-take == [null] [pop pop []] [[str-uncons] dip pred] [cons] linrec
```


#### `dureco`

```joy
dureco == dup rest cons
```


#### `rep-maker`

```joy
rep-maker == [infra dureco] cons cons  dureco
```


#### `rep-first`

```joy
rep-first == first first
```


#### `rep-rest`

```joy
rep-rest == i
```


#### `rep-ints`

```joy
rep-ints == 0 [succ] rep-maker
```


#### `rep-evns`

```joy
rep-evns == 0 [2 +] rep-maker
```


#### `rep-pows`

```joy
rep-pows == 1 [2 *] rep-maker
```


#### `rep-prim`

```joy
rep-prim == 2 [succ [prime not] [succ] while] rep-maker
```


#### `rep-brak`

```joy
rep-brak == [] [[] cons] rep-maker
```


#### `rep-uncons`

```joy
rep-uncons == [rep-first] [rep-rest]  cleave
```


#### `rep-third`

```joy
rep-third == rep-rest rep-rest rep-rest rep-first
```


#### `rep-tenth`

```joy
rep-tenth == 10 [rep-rest] times rep-first
```


#### `rep-drop`

```joy
rep-drop == [rep-rest] times
```


#### `rep-n-th`

```joy
rep-n-th == rep-drop rep-first
```


#### `rep-take`

```joy
rep-take == [null] [pop pop []] [[rep-uncons] dip pred] [cons] linrec
```


#### `rep-n-th-pow`

```joy
rep-n-th-pow == [0 1] [[succ [2 *] dip] infra] rep-maker
```


---

## lazlib.joy

### Predicates

#### `Null`

```joy
Null == null
```


### Operators

#### `First`

```joy
First == first
```


#### `Rest`

```joy
Rest == rest first i
```


#### `Uncons`

```joy
Uncons == uncons first i
```


#### `Cons`

```joy
Cons == [] cons cons
```


#### `Second`

```joy
Second == Rest First
```


#### `Third`

```joy
Third == Rest Rest First
```


#### `Drop`

```joy
Drop == [Rest] times
```


#### `N-th`

```joy
N-th == pred Drop First
```


#### `Size`

```joy
Size == 0 swap
	[Null not]  [[succ] dip Rest]  while
	pop
```


#### `Take`

```joy
Take ==
    [ Null ]
	[ pop pop [] ]
	[ [Uncons] dip  pred ]
	[ cons ]
	linrec
```


### Constructors

#### `From`

```joy
From == dup succ
	[From] cons
	Cons
```

[f [f' From]]


#### `From-to`

```joy
From-to ==
    [ > ]
	[ pop pop [] ]
	[ [dup succ] dip
	  [From-to] cons cons
	  Cons ]
	ifte
```


### Combinators

#### `From-to-by`

```joy
From-to-by ==
    [ dup [From-to-by] cons
	  swapd cons
	  [dupd i] dip cons
	  Cons ]
	[ [pop >]
	  [pop pop pop] ] dip
	ifte
```


#### `From-by`

```joy
From-by == dupd
	dup [i] dip
	[From-by] cons cons
	Cons
```

[f [B(f) [B] From-by]]


#### `Map`

```joy
Map ==
    [pop Null]
	[[]]
	[ [Uncons] dip
	  dup swapd
	  [Map] cons cons
	  [i] dip
	  Cons ]
	ifte
```


#### `Filter`

```joy
Filter ==
    [pop Null]
	[[]]
	[ dup
	  [ [i not] cons [first] swoncat
	    [Rest]
	    while
	    Uncons ]
	  dip
	  [Filter] cons cons
	  Cons ]
	ifte
```


### Examples

#### `Naturals`

```joy
Naturals == 0 From
```


#### `Positives`

```joy
Positives == 1 From
```


#### `Evens`

```joy
Evens == 0 [2 +] From-by
```


#### `Odds`

```joy
Odds == 1 [2 +] From-by
```


#### `Powers-of-2`

```joy
Powers-of-2 == 1 [2 *] From-by
```


#### `Ones`

```joy
Ones == 1 []    From-by
```


#### `Squares`

```joy
Squares == Naturals [dup *] Map
```


#### `LAZLIB`

```joy
LAZLIB == "lazlib.joy - lazy list library\n"
```


---

## laztst.joy

---

## lsplib.joy

### Eval

#### `eval`

```joy
eval ==
    [ list ]
    [
      unswons
      [ [ QUOTE
	  first ]
	[ LAMBDA
	  dupd cons [CLOSURE] swoncat ]
	[ IF
	  uncons [eval] dip
	  swap
	  [ null ]
	  [ pop second]
	  [ pop first ]
	  ifte eval ]
	[ DEF
	  uncons first swap
	  [ eval ]
	  dip
	  dup
	  [ [[] cons] unary2
	    swons
	    swons ]
	  dip ]
	[ DEFUN
	  uncons
	  [LAMBDA] swoncat
	  [] cons cons
	  [DEF] swoncat
	  eval ]
	[
	  swons [eval] map unswons
	  apply ] ]
      case ]
    [
      [ [numerical] [string] sequor ]
	[ ]
	[
	  dupd swap
	  [
	    [ null ]
	    [ true ]
	    [ first first in ]
	    ifte ]
	  [ [ null ]
	    [ pop ]
	    [
	      first unswons rolldown
	      [ [first] dip = ]
	      [ pop pop first ]
	      [ [ [rest] unary2] dip ]
	      tailrec ]
	    ifte ]
	  [ rest ]
	  tailrec ]
	ifte ]
    ifte
```

end eval


### Apply

#### `apply`

```joy
apply ==
    [ list ]
    [
      unswons
      [ [ CLOSURE
	  unswons call swons
	  uncons swapd uncons
	  [ swap cons
	    swons ]
	  dip eval
	  popd ]
	[ "apply: unknown procedure type -\n"
	  putchars abort ] ]
      case ]
    [
     [ [numerical] [string] sequor ]
      [ ] [
      [ [ CAR first first ]
	[ CDR first rest ]
	[ CONS uncons first cons ]
	[ EQ uncons first equal ]
	[ ATOM first leaf ]
	[ NULL first null ]
	[ LIST                  ]
	[
	  [i] dip call ] ]
      case ]
     ifte ]
    ifte
```

end apply


### Lib

#### `lib0`

```joy
lib0 ==
    [
	  [ [ FOLDR ]
	    [ CLOSURE lib0 [lis ini bin]
	      IF [NULL lis] ini
		 [bin [CAR lis]
		      [FOLDR [CDR lis] ini bin] ] ] ]
	  [ [ FOLDL ]
	    [ CLOSURE lib0 [lis ini bin]
	      IF [NULL lis] ini
		 [FOLDL [CDR lis]
			[bin [CAR lis] ini]
			bin ] ] ]
	  [ [ FOLDR2 ]
	    [ CLOSURE lib0 [l1 l2 ini tern]
	      IF [or [NULL l1] [NULL l2]] ini
		 [ tern [CAR l1] [CAR l2]
		        [FOLDR2 [CDR l1] [CDR l2] ini tern] ] ] ]
	  [ [ RECFOLDR ]
	    [ CLOSURE lib0 [x y bin]
	      IF [ATOM x]
		 [bin x y]
		 [IF [NULL x]
		     y
		     [RECFOLDR [CAR x]
			       [RECFOLDR [CDR x]
					 y
					 bin]
			       bin] ] ] ]
	    ]
```


#### `l-prompt`

```joy
l-prompt == "L: "
```


#### `lisp`

```joy
lisp ==
    [ "\nLisp interpreter\n"
	  "\t\tTo include the Lisp library, type\n"
	  "\t\t\t[ include  \"OK\"  \"lsplib.lsp\" ]\n"
	  "GO\n\n" ]
	putstrings
	lib0
	l-prompt putchars get
	[ [ EXIT ] first equal not ]
	[ eval putln
	  l-prompt putchars get ]
	while
	pop pop
	"exit from Lisp interpreter\n" putchars
```


#### `LSPLIB`

```joy
LSPLIB == "lsplib.joy - (eval-apply) Lisp interpreter\n"
```


---

## lsptst.joy

### Definitions

#### `l-prompt`

```joy
l-prompt == ""
```


---

## mandel.joy

---

## mandel1.joy

### Definitions

#### `nrows`

```joy
nrows == 30
```


#### `ncols`

```joy
ncols == 75
```

size of output display


#### `mandel`

```joy
mandel ==
    -1 nrows ncols *
	  [ succ dup dup [col] dip row
            get_c1 swap get_c2
            get_k putpt
            [eol] ['\n putch] [] ifte
          ] times
```

size of output display


#### `row`

```joy
row == ncols /
```

loop for each cell


#### `col`

```joy
col == ncols rem
```

I:i -> I:row


#### `eol`

```joy
eol == col ncols pred =
```

I:i -> I:col


#### `putpt`

```joy
putpt == 16 rem " .:,;!/>)|&IH%*#" of putch
```

I:i -> B


#### `get_k`

```joy
get_k ==
    pairlist [0 0] swoncat
          0 [112 <= [eoloop] dip and]
          [succ [inc_zs] dip] while
          popd 1 -
```

we've a palette of 16


#### `get_c1`

```joy
get_c1 == 0.10 * 1.5 -
```

cleanup               -> I


#### `get_c2`

```joy
get_c2 == 0.04 * 2.0 -
```

outer loop coeff. I:i -> F


#### `sqr_z1`

```joy
sqr_z1 == first dup *
```

inner loop coeff. I:i -> F


#### `sqr_z2`

```joy
sqr_z2 == 1 at  dup *
```

A -> I


#### `inc_z1`

```joy
inc_z1 == dup [sqr_z1] [sqr_z2] cleave -
          [3 at] dip +
```

A -> I


#### `inc_z2`

```joy
inc_z2 ==
    3 take reverse uncons uncons
          uncons pop
          2 * * +
```

A -> I


#### `eoloop`

```joy
eoloop == dup sqr_z1 swap sqr_z2 + 10 <=
```

A -> I


#### `inc_zs`

```joy
inc_zs ==
    dup [inc_z1] [inc_z2] cleave
          pairlist swap rest rest concat
```

(z1*z1)+(z2*z2)<=10 A -> B


---

## mandel2.joy

### Definitions

#### `mandel`

```joy
mandel ==
    0 30 from-to-list [-0.10 * 1.5 +] map
    0 74 from-to-list [-0.04 * 1.0 +] map
    cartproduct 31 slices
    [[putpt] step newline] step
```


#### `putpt`

```joy
putpt == get_k 16 rem " .:,;!/>)|&IH%*#" of putch
```


#### `inczs`

```joy
inczs ==
    [[* 2 *] nullary rollup [dup *] unary2
    [+ 10 >] [pop2 pop] [- swap] ifte] infra
```


#### `get_k`

```joy
get_k ==
    [-1 [0 0]] dip [[+] mapr2 inczs [succ] dip] cons
    [null not swap 112 < and] repeat pop
```


#### `slices`

```joy
slices ==
    [reverse dup size [] rollup] dip /
    [dup swapd [] rollup
      [swapd [unswons] dipd consd swapd pred]
      [0 >] repeat
      pop [rollup consd] dip swap]
    [pop null not] repeat
    pop2
```


---

## mapreduce.joy

### Definitions

#### `pair`

```joy
pair == [] cons cons
```


#### `fst`

```joy
fst == first
```


#### `snd`

```joy
snd == rest first
```


#### `sum-values`

```joy
sum-values == 0 [+] fold
```


#### `in-list`

```joy
in-list == swap [=] cons some
```

in-list: elem list -> bool


#### `unique-step`

```joy
unique-step == [swap in-list] [pop] [swons] ifte
```

unique: list -> list with duplicates removed


#### `unique`

```joy
unique == [] [unique-step] fold
```


#### `get-keys`

```joy
get-keys == [fst] map unique
```

get-keys: [[k v]...] -> [k...] unique keys


#### `vals-for-key`

```joy
vals-for-key == [swap fst =] cons filter [snd] map
```

vals-for-key: pairs key -> [v...] values for that key


#### `group-one`

```joy
group-one == dup rollup vals-for-key pair
```

group-one: pairs key -> [key [values]]


#### `group-by-key`

```joy
group-by-key == dup get-keys [group-one] map [pop] dip
```

group-by-key: [[k v]...] -> [[k [vs]]...]


#### `sum-reducer`

```joy
sum-reducer == dup fst swap snd sum-values pair
```


#### `count-reducer`

```joy
count-reducer == dup fst swap snd size pair
```


#### `map-reduce`

```joy
map-reduce ==
    rolldown swap [pmap] dip
        swap flatten swap
        [group-by-key] dip pmap
```


---

## modtst.joy

### Definitions

#### `zero`

```joy
zero == 0 0 pop
```


#### `one`

```joy
one == zero 1 +
```


#### `un`

```joy
un == one
```


#### `deux`

```joy
deux == zero one un + +
			IN
	two == deux
```


#### `two`

```joy
two == deux
```


#### `three`

```joy
three == zero un + deux +
```


#### `four`

```joy
four == two three + one -
```


#### `trois`

```joy
trois == two succ
```


#### `eins`

```joy
eins == one
```


#### `zwei`

```joy
zwei == one dup +
```


#### `drei`

```joy
drei == eins zwei +
```


#### `quatre`

```joy
quatre == drei eins +
```


#### `cinq`

```joy
cinq == two trois +
			IN
	five == cinq
```


#### `five`

```joy
five == cinq
```


#### `six`

```joy
six == trois three +
```


#### `seven`

```joy
seven == trois drei + succ
```


#### `twenty`

```joy
twenty == 20
```


#### `thirty`

```joy
thirty == 30
```


#### `fifty`

```joy
fifty == twenty thirty +
```


#### `a`

```joy
a == "a"
```


#### `b`

```joy
b == "b"
    PUBLIC
	ab == a b concat
```


#### `ab`

```joy
ab == a b concat
```


#### `ba`

```joy
ba == b a concat
```


#### `abba`

```joy
abba == ab ba concat
```


#### `a`

```joy
a == "A"
```


#### `b`

```joy
b == "B"
    PUBLIC
	ab == a b concat
```


#### `ab`

```joy
ab == a b concat
```


#### `ba`

```joy
ba == b a concat
```


#### `abba`

```joy
abba == ab ba concat
```


#### `c`

```joy
c == "C"
```


#### `d`

```joy
d == "D"
	    IN
	cd == c d concat
```


#### `cd`

```joy
cd == c d concat
```


#### `abc`

```joy
abc == a b concat c concat
```


#### `bcd`

```joy
bcd == b cd concat
```


---

## mthlib.joy

### Definitions

#### `kons`

```joy
kons == [] cons cons [of] cons
```

"object oriented" lists: 	constructor kons, 	selectors kar,kdr (messages)


#### `kar`

```joy
kar == 0 swap i
```


#### `kdr`

```joy
kdr == 1 swap i
```


### Numericalcalculator

#### `calc`

```joy
calc ==
    [ numerical ]
	[ ]
	[ unswons
	  [ dup [+ - * /] in ]
	  [ [ [calc] map uncons first ] dip
	    call ]
	  [ "bad operator\n" put ]
	  ifte ]
	ifte
```

e.g.   [* 10 [+ 2.2 3.3]]  calc.


### Symbolicdifferentiation

#### `diff`

```joy
diff ==
    [ [ [numerical]
	    pop pop 0 ]
	  [ [leaf]
	    [[first] dip = ]
	    [pop pop 1]
	    [pop pop 0]
	    ifte ]
	  [ [list]
	    unswons
	    [ [ +
		pop unpair
		[diff] unary2
		_makesum popd ]
	      [ *
		pop unpair
		dup2
		[[diff] unary2] dip2
		rollup
		_makeproduct [_makeproduct] dip
		_makesum popd ]
	      [] ]
	    opcase i ]
	  [] ]
	cond
```


#### `MTHLIB`

```joy
MTHLIB == "mthlib.joy - symbolic mathematics library\n"
```


---

## mtrlib.joy

### Predicate

#### `vv-samesize`

```joy
vv-samesize == [size] dip size =
```


### Operators

#### `n-e-vector`

```joy
n-e-vector == [] cons [times] cons cons
                [] swap infra
```


#### `v-negate-v`

```joy
v-negate-v == 0 swap [-]sv-bin-v
```


#### `v-invert-v`

```joy
v-invert-v == 1.0 swap [/]sv-bin-v
```


### Combinators

#### `sv-bin-v`

```joy
sv-bin-v == map popd
```


#### `vs-bin-v`

```joy
vs-bin-v == cons map
```


#### `vs-cbin-v`

```joy
vs-cbin-v == swapd map popd
```

efficiency


### Predicate

#### `vv-comformable`

```joy
vv-comformable == [size] dip size =
```


### Combinators

#### `vv-bin-v`

```joy
vv-bin-v == mapr2
```


#### `vv-2bin-s`

```joy
vv-2bin-s == [mapr2 uncons] dip step
```


### Unary Operators

#### `v-1row-m`

```joy
v-1row-m == [] cons
```

unary operators


#### `v-1col-m`

```joy
v-1col-m == [] swap [swons] sv-bin-v
```


#### `v-zdiag-m`

```joy
v-zdiag-m ==
    [ small ]
    [ [null]  []  [[] cons]  ifte ]
    [ uncons ]
    [ dup  [first rest 0 swons cons]  dip
      [0 swons]  map
      cons ]
    linrec
```


#### `v-e-diag-m`

```joy
v-e-diag-m ==
    dup
	[ [swons cons] cons [first rest] swoncat ] dip
	[swons] cons
	[map cons] cons [dip] swoncat cons [dup] swoncat
      	[ [small]
      	  [[null] [] [[] cons] ifte]
      	  [uncons] ]  dip
      	linrec
```


### Combinator

#### `vv-bin-m`

```joy
vv-bin-m == [map popd] cons cons map
```


#### `m-dimensions`

```joy
m-dimensions == dup [size] dip first size
```


#### `mm-conformable`

```joy
mm-conformable == [size] dip first size =
```


#### `mm-samedimensions`

```joy
mm-samedimensions ==
    [m-dimensions] dip m-dimensions
                      swapd  =  [=] dip and
```


### Operators

#### `mm-vercat-m`

```joy
mm-vercat-m == concat
```


#### `mm-horcat-m`

```joy
mm-horcat-m == [concat] mapr2
```


#### `m-transpose-m`

```joy
m-transpose-m ==
    [ [null] [true] [[null] some] ifte ]
	[ pop [] ]
	[ [[first] map] [[rest] map] cleave ]
	[ cons ]
	linrec
```


#### `sm-bin-m`

```joy
sm-bin-m == [sv-bin-v] cons map popd
```

matrices and scalars


#### `ms-bin-m`

```joy
ms-bin-m == cons [map] cons map
```


#### `ms-cbin-m`

```joy
ms-cbin-m == swapd sm-bin-m
```

efficiency


#### `mm-bin-m`

```joy
mm-bin-m == [mapr2] cons mapr2
```

two matrices


#### `mm-add-m`

```joy
mm-add-m == [+] mm-bin-m
```


#### `mm-mul-m`

```joy
mm-mul-m == transpose
	[ [[*] mapr2 0 [+] fold] map popd ] cons
	map
```


#### `mm-2bin-m`

```joy
mm-2bin-m ==
    [fold] cons [[mapr2 unswons] cons] dip concat
	[ transpose ] dip
	[ map popd ] cons cons
	map
```


#### `m-print`

```joy
m-print == putlist
```


#### `MTRLIB`

```joy
MTRLIB == "mtrlib.joy - matrix/vector library\n"
```

"[ " putchars 	[ null ] 	[ pop ] 	[ unswons 	  put 	  [ "\n  " putchars put ] step ] 	ifte 	"]\n" putchars.


---

## mtrtst.joy

### Inner Or Scalar Product

#### `id2`

```joy
id2 == 2 1 n-e-vector  0 v-e-diag-m
```

2 by 2 identity matrix


---

## numlib.joy

### Predicates

#### `positive`

```joy
positive == 0 >
```


#### `negative`

```joy
negative == 0 <
```


#### `even`

```joy
even == 2 rem null
```


#### `odd`

```joy
odd == even not
```


#### `prime`

```joy
prime ==
    2
	[ [dup * >] nullary  [rem 0 >] dip  and ]
	[ succ ]
	while
	dup * <
```


### Functions

#### `fact`

```joy
fact == [1 1] dip [dup [*] dip succ] times pop
```


#### `fib`

```joy
fib == [1 0] dip [swap [+] unary] times popd
```


#### `gcd`

```joy
gcd == [0 >] [dup rollup rem] while pop
```


#### `fahrenheit`

```joy
fahrenheit == 9 * 5 / 32 +
```


#### `celsius`

```joy
celsius == 32 - 5 * 9 /
```


#### `pi`

```joy
pi == 3.14159265
```


#### `e`

```joy
e == 1.0 exp
```


#### `radians`

```joy
radians == pi * 180 /
```


#### `sindeg`

```joy
sindeg == radians sin
```


#### `cosdeg`

```joy
cosdeg == radians cos
```


#### `tandeg`

```joy
tandeg == radians tan
```


#### `qroots`

```joy
qroots ==
    [	pop pop null ]
	[	[   pop null ]
		[   [ null   ]
		    [ [_INF] ]
		    [ [] ]
		    ifte
		    [ pop pop pop ] dip ]
		[   0 swap - swap 1.0 * /
		    [] cons popd ]
		ifte ]
	[	[   [ dup * swap ] dip
		      4 * * - ] unary
		[   0 < ]
		[   pop pop pop [_COMPLEX] ]
		[   [ 0 swap - 1.0 *
		      swap 2 * ] dip
		    [ 0 = ]
		    [ pop / [] cons ]
		    [ sqrt swapd dup2 - [+] dip
			[] cons cons
		      swap [/] cons map ]
		    ifte ]
		ifte ]
	ifte
```

find roots of the quadratic equation with coefficients a b c : 				  a * X^2  +  b * X   +  c  =  0


#### `quadratic-formula`

```joy
quadratic-formula ==
    [ [ [ pop pop 2 * ]
            [ pop 0 swap - ]
            [ swap dup * rollup * 4 * - sqrt] ]
          [i] map ]
        ternary i
        [ [ [ + swap / ]
            [ - swap / ] ]
          [i] map ]
        ternary
```

a simpler version, FEB 05 :


### Combinators

#### `deriv`

```joy
deriv == [unary2 swap - 0.001 /] cons  [dup 0.001 +] swoncat
```


#### `newton`

```joy
newton ==
    dup deriv
	[ pop i abs 0.0001 > ]
	[ [dupd] dip
	  dup2
	  [[cleave / - ] dip]
	  dip  ]
	while
	pop pop
```


#### `use-newton`

```joy
use-newton == [[-] cons] dip  swoncat  1 swap newton
```


#### `cube-root`

```joy
cube-root == [dup dup * *] use-newton
```


#### `NUMLIB`

```joy
NUMLIB == "numlib.joy - numerical library\n"
```


---

## plglib.joy

### Definitions

#### `unops`

```joy
unops == [ not - N ]
```


#### `binops`

```joy
binops == [ imp > C  iff = E  or v A  and & K ]
```


#### `bin1ops`

```joy
bin1ops == [ imp > C   iff = E ]
```


#### `bin2ops`

```joy
bin2ops == [ or v A ]
```


#### `bin3ops`

```joy
bin3ops == [ and & K ]
```


### Semantictableaux

#### `show-all`

```joy
show-all ==
    pop
	[ [null]
	  ["\tnot tautology, countermodel(s):\n" putchars]
	  []
	  ifte
	  "  " putchars succ dup put ]
	dipd
	swap '\t putch
	"T: " putchars put
	"  F: " putchars putln
```


#### `show-first`

```joy
show-first ==
    pop
	"\tnot tautology, first countermodel:\n" putchars
	swap '\t putch
	"T: " putchars put
	"  F: " putchars putln
	succ
```


#### `collect`

```joy
collect == pop [] cons cons swons
```


#### `count`

```joy
count == pop pop pop succ
```


#### `found`

```joy
found ==
    pop pop pop not
IN
    ver-all ==
	"ver-all B" t-trace
	unswons
	[ [ [[or v A] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip ver-all] dip ]
	    dip
	    [i] dip ver-all ]
	  [ [[imp > C] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip fal-all] dip ]
	    dip
	    [i] dip ver-all ]
	  [ [[iff = E] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    dup
	    [[and] swoncat] dip   [or] swoncat
	    [ swap
	      [[i] dip ver-all] dip ]
	    dip
	    [i] dip fal-all ]
	  [ [[and & K] in]
	    pop uncons first swap
	    [ [ver-all] cons swons ]
	    dip ver-all ]
	  [ [[not - N] in]
	    pop first fal-all ]
	  [ popd
	    [ popd has ]
	    [ pop pop pop pop ]
	    [ swap
	      [ swapd
		[has] [pop] [swons] ifte
		swap ]
	      dip
	      unswons i ]
	    ifte ] ]
	cond
	"ver-all E" t-trace
```


#### `ver-all`

```joy
ver-all ==
    "ver-all B" t-trace
	unswons
	[ [ [[or v A] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip ver-all] dip ]
	    dip
	    [i] dip ver-all ]
	  [ [[imp > C] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip fal-all] dip ]
	    dip
	    [i] dip ver-all ]
	  [ [[iff = E] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    dup
	    [[and] swoncat] dip   [or] swoncat
	    [ swap
	      [[i] dip ver-all] dip ]
	    dip
	    [i] dip fal-all ]
	  [ [[and & K] in]
	    pop uncons first swap
	    [ [ver-all] cons swons ]
	    dip ver-all ]
	  [ [[not - N] in]
	    pop first fal-all ]
	  [ popd
	    [ popd has ]
	    [ pop pop pop pop ]
	    [ swap
	      [ swapd
		[has] [pop] [swons] ifte
		swap ]
	      dip
	      unswons i ]
	    ifte ] ]
	cond
	"ver-all E" t-trace
```


#### `fal-all`

```joy
fal-all ==
    "fal-all B" t-trace
	unswons
	[ [ [[and & K] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip fal-all] dip ]
	    dip
	    [i] dip fal-all ]
	  [ [[or v A] in]
	    pop uncons first swap
	    [ [fal-all] cons swons ]
	    dip fal-all ]
	  [ [[imp > C] in]
	    pop uncons first swap
	    [ [fal-all] cons swons ]
	    dip ver-all ]
	  [ [[iff = E] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    dup
	    [[imp] swoncat] dip   [swap] infra [imp] swoncat
	    [ swap
	      [[i] dip fal-all] dip ]
	    dip
	    [i] dip fal-all ]
	  [ [[not - N] in]
	    pop first ver-all ]
	  [ popd
	    [ popd popd has ]
	    [ pop pop pop pop ]
	    [ swap
	      [ [has] [pop] [swons] ifte ]
	      dip
	      unswons i ]
	    ifte ] ]
	cond
	"fal-all E" t-trace
```


#### `ver-first`

```joy
ver-first ==
    [ pop pop pop pop null ]
      [
	"ver-first B" t-trace
	unswons
	[ [ [[or v A] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip ver-first] dip ]
	    dip
	    [i] dip ver-first ]
	  [ [[imp > C] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip fal-first] dip ]
	    dip
	    [i] dip ver-first ]
	  [ [[iff = E] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    dup
	    [[and] swoncat] dip   [or] swoncat
	    [ swap
	      [[i] dip ver-first] dip ]
	    dip
	    [i] dip fal-first ]
	  [ [[and & K] in]
	    pop uncons first swap
	    [ [ver-first] cons swons ]
	    dip ver-first ]
	  [ [[not - N] in]
	    pop first fal-first ]
	  [ popd
	    [ popd has ]
	    [ pop pop pop pop ]
	    [ swap
	      [ swapd
		[has] [pop] [swons] ifte
		swap ]
	      dip
	      unswons i ]
	    ifte ] ]
	cond
	"ver-first E" t-trace
	]
      [ pop pop pop pop ]
      ifte
```


#### `fal-first`

```joy
fal-first ==
    [ pop pop pop pop null ]
      [
	"fal-first B" t-trace
	unswons
	[ [ [[and & K] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    uncons first
	    [ swap
	      [[i] dip fal-first] dip ]
	    dip
	    [i] dip fal-first ]
	  [ [[iff = E] in]
	    pop
	    [ [] cons cons cons dup ] dip
	    dup
	    [[imp] swoncat] dip   [swap] infra [imp] swoncat
	    [ swap
	      [[i] dip fal-first] dip ]
	    dip
	    [i] dip fal-first ]
	  [ [[or v A] in]
	    pop uncons first swap
	    [ [fal-first] cons swons ]
	    dip fal-first ]
	  [ [[imp > C] in]
	    pop uncons first swap
	    [ [fal-first] cons swons ]
	    dip ver-first ]
	  [ [[not - N] in]
	    pop first ver-first ]
	  [ popd
	    [ popd popd has ]
	    [ pop pop pop pop ]
	    [ swap
	      [ [has] [pop] [swons] ifte ]
	      dip
	      unswons i ]
	    ifte ] ]
	cond
	"fal-first E" t-trace
	]
      [ pop pop pop pop ]
      ifte
```


#### `t-trace-default`

```joy
t-trace-default == putchars " : " putchars stack putln
```


#### `t-trace`

```joy
t-trace == pop
```


#### `taut-show-all`

```joy
taut-show-all ==
    [ 0  [] [] [[show-all]] ] dip
	fal-all
	[null] ['\t putch "tautology\n" putchars] [] ifte
	pop
```


#### `taut-show-first`

```joy
taut-show-first ==
    [ 0  [] [] [[show-first]] ] dip
	fal-first
	[null] ['\t putch "tautology\n" putchars] [] ifte
	pop
```


#### `taut-collect-all`

```joy
taut-collect-all == [ [] [] [] [[collect]] ] dip
	fal-all
```


#### `taut-collect-first`

```joy
taut-collect-first == [ [] [] [] [[collect]] ] dip
	fal-first
```


#### `taut-count`

```joy
taut-count == [ 0 [] [] [[count]] ] dip
	fal-all
```


#### `taut-test`

```joy
taut-test == [ false [] [] [[found]] ] dip
	fal-first not
```


### Other Stuff To Go Here

#### `PLGLIB`

```joy
PLGLIB == "plglib.joy - propositional logic library\n"
```

other stuff to go here


---

## plgtst.joy

### Definitions

#### `m-show`

```joy
m-show == Min2Tre taut-show-all
```


#### `p-show`

```joy
p-show == Pol2Tre taut-show-all
```


#### `r-show`

```joy
r-show == Rev2Tre taut-show-all
```


#### `m-show1`

```joy
m-show1 == Min2Tre taut-show-first
```


#### `m-collect`

```joy
m-collect == Min2Tre taut-collect-all
```


#### `m-collect1`

```joy
m-collect1 == Min2Tre taut-collect-first
```


#### `m-count`

```joy
m-count == Min2Tre taut-count
```


#### `m-test`

```joy
m-test == Min2Tre taut-test
```


#### `big-formula`

```joy
big-formula ==
    [  [ CEO-sells-shares
	imp    insider-trading  and  traders-conspire ]		and
     [ i-trade-exposed
	imp    CEO-resigns  or  CEO-is-sacked ]			and
     [ CEO-resigns
	imp    i-trade-exposed  and  shares-drop ]		and
     [ traders-conspire  and  CEO-is-sacked
	imp    sales-drop ]					and
     [ sales-drop
	imp    CEO-is-sacked  and  shares-drop ]
  imp
     [ CEO-sells-shares
	imp    shares-drop ]  ]
```


---

## prelib.joy

### Definitions

#### `app1`

```joy
app1 == i
```


#### `app2`

```joy
app2 == unary2
```


#### `app3`

```joy
app3 == unary3
```


#### `app4`

```joy
app4 == unary4
```


#### `app11`

```joy
app11 == i popd
```


#### `app12`

```joy
app12 == unary2 rolldown pop
```


#### `fputstring`

```joy
fputstring == fputchars
```


#### `fold`

```joy
fold == swapd step
```


#### `enconcat`

```joy
enconcat == swapd cons concat
```


#### `inf`

```joy
inf == 1 1024 ldexp
```


---

## quad.joy

### Definitions

#### `quadratic-2`

```joy
quadratic-2 ==
    [ [ [ pop pop 2 * ]
            [ pop 0 swap - ]
            [ swap dup * rollup * 4 * - sqrt] ]
          [i] map ]
        ternary i
        [ [ [ + swap / ]
            [ - swap / ] ]
          [i] map ]
        ternary
```


---

## quadratic.joy

### Definitions

#### `quadratic-1`

```joy
quadratic-1 ==
    [ [ pop pop 2 * ]
          [ pop 0 swap - ]
          [ swap dup * rollup * 4 * - sqrt] ]
        [i] map [] step
        [ [ + swap / ]
          [ - swap / ] ]
        [i] map  [] step
        [] cons cons
        [ pop pop pop pop pop pop ] dip
```


#### `quadratic-2`

```joy
quadratic-2 ==
    [ [ [ pop pop 2 * ]
            [ pop 0 swap - ]
            [ swap dup * rollup * 4 * - sqrt] ]
          [i] map ]
        ternary i
        [ [ [ + swap / ]
            [ - swap / ] ]
          [i] map ]
        ternary
```


#### `quadratic-3`

```joy
quadratic-3 ==
    [] cons cons cons
        [ [ [ [dup * swap] dip * 4 * - sqrt ]
            [ pop 0 swap - ]
            [ 2 * ] ]
          [i] map ]
        infra first
        [ [ [ + swap / ]
            [ - swap / ] ]
          [i] map ]
        infra first
```


---

## replib.joy

### Definitions

#### `foo`

```joy
foo == "foo"
PUBLIC
    duco == dup cons
```


#### `duco`

```joy
duco == dup cons
```


#### `dureco`

```joy
dureco == dup rest cons
```


#### `durereco`

```joy
durereco == dup rest rest cons
```


#### `count`

```joy
count == [0 [succ] infra] swoncat
```


#### `deposit`

```joy
deposit == [dup [first] dip] swoncat
```


#### `self`

```joy
self == [duco]         duco
```


#### `ints`

```joy
ints == [dureco] count dureco
```


#### `exe`

```joy
exe == [dip duco]   cons       duco
```


#### `exe-c`

```joy
exe-c == [dip dureco] cons count dureco
```


#### `c-stream`

```joy
c-stream == [dureco] cons dureco
```


#### `c-stream-d`

```joy
c-stream-d == [dureco] deposit cons dureco
```


#### `n-stream`

```joy
n-stream == [infra dureco] cons cons dureco
```


#### `n-stream-d`

```joy
n-stream-d == [infra dureco] cons deposit cons dureco
```


#### `f-stream-prepare`

```joy
f-stream-prepare ==
    swap
        [dip] cons concat
        cons
        [dup] infra
        dup rest rest infra
        uncons uncons
        [pop dup] swoncat
        [infra durereco] cons
```


#### `f-stream`

```joy
f-stream == f-stream-prepare cons cons durereco
```


#### `f-stream-d`

```joy
f-stream-d == f-stream-prepare deposit cons cons durereco
```


#### `inter`

```joy
inter ==
    [dip cons dureco] cons
        [uncons] swoncat
        cons dureco
```


#### `exe-t`

```joy
exe-t ==
    [dip dureco] cons
        [[pred] infra] swoncat
        [ifte] cons
        [pop [[duco] duco]] swons
        [first null] swons
        cons dureco
```


#### `fix`

```joy
fix == [duco] swoncat duco
```


#### `fix-c`

```joy
fix-c == [0 [succ] infra dureco] swoncat dureco
```


#### `fix-i`

```joy
fix-i ==
    [dip cons dureco] cons
        [uncons] swoncat
        cons
        swoncat
        dureco
```


#### `fix-a`

```joy
fix-a == [] [[cons] unary] fix-i
```


#### `linear`

```joy
linear == [i] _expand
```


#### `binary`

```joy
binary == [dip swap i] _expand
```


#### `REPLIB`

```joy
REPLIB == "replib.joy - for survey of replicating programs"
```


---

## reptst.joy

### Definitions

#### `selfr`

```joy
selfr == rep.self
```


#### `squaring`

```joy
squaring == [dup *] rep.exe
```


#### `state`

```joy
state == first first
```


#### `integers`

```joy
integers == rep.ints
```


#### `times10-c`

```joy
times10-c == [10 *] rep.exe-c
```


#### `ones`

```joy
ones == 1 rep.c-stream
```


#### `halving`

```joy
halving == 1.0 [2 /] rep.n-stream
```


#### `integers-from`

```joy
integers-from == [succ] rep.n-stream
```


#### `ones-d`

```joy
ones-d == 1 rep.c-stream-d
```


#### `halving-d`

```joy
halving-d == 1.0 [2 /] rep.n-stream-d
```


#### `primes-d`

```joy
primes-d == 2 [succ [prime not] [succ] while] rep.n-stream-d
```


#### `even-squares`

```joy
even-squares == 0 [2 +] [dup *] rep.f-stream
```


#### `ten-powers-log10`

```joy
ten-powers-log10 == 1 [10 *] [[] cons [dup log10] infra]  rep.f-stream-d
```


#### `accu-list`

```joy
accu-list == [] [cons] rep.inter
```


#### `accu-sum`

```joy
accu-sum == 0 [+] rep.inter
```


#### `accu-product-list`

```joy
accu-product-list == [] [[*] dip cons] rep.inter
```


#### `max-3-adds`

```joy
max-3-adds == 3 [+] rep.exe-t
```


#### `max-4-adds`

```joy
max-4-adds == 4 [+] rep.exe-t
```


#### `fact0`

```joy
fact0 == [[pop null] [pop pop 1  ] [[dup pred] dip i  *     ] ifte]
```


#### `fact`

```joy
fact == [[pop null] [[pop 1] dip] [[dup pred] dip i [*] dip] ifte]
```


#### `fact-fix`

```joy
fact-fix == fact rep.fix
```


#### `fact-fix-c`

```joy
fact-fix-c == fact rep.fix-c
```


#### `fact-fix-a`

```joy
fact-fix-a == fact rep.fix-a
```


#### `steps`

```joy
steps == "steps: " putchars state putln
```


#### `trace`

```joy
trace == "trace: " putchars state putln
```


#### `nfib`

```joy
nfib ==
    [ [ pop small ]
	  [ [pop 1] dip ]
	  [ [pred dup pred] dip
	    dip swap i
	    [+] dip ]
	  ifte ]
```


#### `nfib-fix`

```joy
nfib-fix == nfib rep.fix
```


#### `nfib-fix-c`

```joy
nfib-fix-c == nfib rep.fix-c
```


#### `nfib-fix-a`

```joy
nfib-fix-a == nfib rep.fix-a
```


#### `fact-lin`

```joy
fact-lin == [null] [pop 1] [dup pred] [*] rep.linear
```


#### `length-lin`

```joy
length-lin == [null] [pop 0] [rest] [succ] rep.linear
```


#### `nfib-bin`

```joy
nfib-bin == [small] [pop 1] [pred dup pred] [+] rep.binary
```


#### `fact-fix`

```joy
fact-fix == fact-lin rep.fix
```


#### `length-fix-a`

```joy
length-fix-a == length-lin rep.fix-a
```


#### `nfib-fix`

```joy
nfib-fix == nfib-bin rep.fix
```


#### `qsort-bin`

```joy
qsort-bin == [small] [] [uncons [>] split] [enconcat] rep.binary
```


#### `qsort-fix-c`

```joy
qsort-fix-c == qsort-bin rep.fix-c
```


#### `qtest`

```joy
qtest == qsort-fix-c i state
```


---

## seqlib.joy

### Definitions

#### `putlist`

```joy
putlist ==
    "[ " putchars
	[ null ]
	[ pop ]
	[ unswons
	  put
	  [ "\n  " putchars put ] step ]
	ifte
	"]\n" putchars
```


#### `reverse`

```joy
reverse == [[]] [""] iflist swap shunt
```


#### `reverselist`

```joy
reverselist == [] swap shunt
```


#### `reversestring`

```joy
reversestring == "" swap shunt
```


#### `flatten`

```joy
flatten == [null] [] [uncons] [concat] linrec
```


#### `restlist`

```joy
restlist == [null] [[] cons] [dup rest] [cons] linrec
```


#### `product`

```joy
product == 1 [*] fold
```


#### `scalarproduct`

```joy
scalarproduct == [0] dip2
	[null2] [pop2] [uncons2 [* +] dip2] tailrec
```


#### `frontlist1`

```joy
frontlist1 ==
    [null] [[] cons]
	[uncons]
	[ [cons] map popd [] swons ]
	linrec
```


#### `frontlist`

```joy
frontlist ==
    [null] [[] cons]
	[uncons]
	[ [cons] map popd dup first rest swons ]
	linrec
```


#### `subseqlist`

```joy
subseqlist ==
    [null]
	[[] cons]
	[ uncons dup
	  [frontlist [cons] map popd] dip ]
        [concat]
	linrec
```


#### `powerlist1`

```joy
powerlist1 ==
    [null] [[] cons] [uncons]
	[dup swapd [cons] map popd concat] linrec
```


#### `powerlist2`

```joy
powerlist2 ==
    [null] [[] cons] [uncons]
	[dup swapd [cons] map popd swoncat] linrec
```


#### `insertlist`

```joy
insertlist ==
    swons
	[ small ]
	[ unitlist ]
	[ dup
	  unswons unconsd swons ]
	[ swap [swons] cons map
	  cons ]
	linrec
```


#### `permlist`

```joy
permlist ==
    [ small ]
	[ unitlist ]
	[ uncons ]
	[ swap [insertlist] cons map
	  flatten ]
	linrec
```


#### `qsort`

```joy
qsort == [small] [] [uncons [>] split] [swapd cons concat] binrec
```


#### `qsort1-1`

```joy
qsort1-1 ==
    [small]
	[]
	[uncons unswonsd [first >] split [swons] dip2]
	[swapd cons concat]
	binrec
```


#### `qsort1`

```joy
qsort1 ==
    [small] [] [uncons [[first] unary2 >] split] [swapd cons concat]
        binrec
```


#### `mk_qsort`

```joy
mk_qsort ==
    [ [small] [] ] dip
	[ unary2 >] cons [split] cons [uncons] swoncat
	[ swapd cons concat ]
	binrec
```


#### `merge`

```joy
merge ==
    [ [ [null] [pop] ]
	  [ [pop null] [popd] ]
	  [ [unswons2 <] [unconsd] [cons] ]
	  [ [unswons2 >] [uncons swapd] [cons] ]
	  [ [uncons2] [cons cons] ] ]
	condlinrec
```


#### `merge1`

```joy
merge1 ==
    [ [ [null] [pop] ]
	  [ [pop null] [popd] ]
	  [ [unswons2 [first] unary2 <] [unconsd] [cons] ]
	  [ [unswons2 [first] unary2 >] [uncons swapd] [cons] ]
	  [ [uncons2] [cons cons] ] ]
	condlinrec
```


#### `insert`

```joy
insert ==
    [pop null] [firstd >=] disjoin
	[ swons ]
	[ unconsd]
	[ cons ]
	linrec
```


#### `insert-old`

```joy
insert-old ==
    [ [ [pop null] [swons] ]
	  [ [firstd >= ] [swons] ]
	  [ [unconsd] [cons] ] ]
	condlinrec
```


#### `delete`

```joy
delete ==
    [ [ [pop null] [pop] ]
	  [ [firstd >] [pop] ]
	  [ [firstd =] [pop rest] ]
	  [ [unconsd] [cons] ] ]
	condlinrec
```


#### `transpose`

```joy
transpose ==
    [ [null] [true] [[null] some] ifte ]
	[ pop [] ]
	[ [[first] map] [[rest] map] cleave ]
	[ cons ]
	linrec
```


#### `cartproduct`

```joy
cartproduct == [[]] dip2 [pairlist swap [swons] dip] pairstep
```


#### `orlist`

```joy
orlist == [list] swap disjoin
```


#### `orlistfilter`

```joy
orlistfilter == orlist [filter] cons
```


#### `treeshunt`

```joy
treeshunt == [swons] treestep
```


#### `treeflatten`

```joy
treeflatten == [] swap treeshunt reverse
```


#### `treereverse`

```joy
treereverse == [] [reverse] [map] treegenrec
```


#### `treestrip`

```joy
treestrip == [list] treefilter
```


#### `treemap`

```joy
treemap == [] [map] treegenrec
```


#### `treefilter`

```joy
treefilter == [] swap orlistfilter [map] treegenrec
```


#### `treesample`

```joy
treesample == [ [1 2 [3 4] 5 [[[6]]] 7 ] 8 ]
```


#### `SEQLIB`

```joy
SEQLIB == "seqlib.joy - sequence library, assumes agglib.joy\n"
```


---

## symlib.joy

### Definitions

#### `C2T`

```joy
C2T ==
    [ list ]
	[ unswons
	  [ [ [unops in]
	      [first C2T [] cons] dip swons ]
	    [ [[QUOTE] in]
	      swons ]
	    [ [uncons first [C2T] dip C2T [] cons cons]
	      dip swons ] ]
	  cond ]
	[ [] cons ]
	ifte
```

from Cambridge to ..


#### `C2I`

```joy
C2I ==
    [ list ]
	[ unswons
	  [ [ [unops in]
	      [first C2I] dip swons ]
	    [ [[QUOTE] in]
	      swons ]
	    [ [uncons first [C2I] dip C2I]
	      dip swons concat [] cons ] ]
	  cond ]
	[ [] cons ]
	ifte
```


#### `C2P`

```joy
C2P ==
    [ list ]
	[ unswons
	  [ [ [unops in]
	      [first C2P] dip ]
	    [ [[QUOTE] in]
	      pop first ]
	    [ [uncons first swap [C2P] dip C2P]
	      dip ] ]
	  cond ]
	[ ]
	ifte
```


#### `C2R`

```joy
C2R ==
    [ list ]
	[ unswons
	  [ [ [unops in]
	      swap first C2R ]
	    [ [[QUOTE] in]
	      pop first ]
	    [ swap uncons first swap
	      [C2R] dip C2R ] ]
	  cond ]
	[ ]
	ifte
```


#### `I2C`

```joy
I2C ==
    unswons
	[ [ [unops in]
	    [I2C [] cons] dip swons ]
	  [ [[QUOTE] in]
	    [unswons [] cons] dip swons ]
	  [ [list]
	    I2C swap uncons swapd
	    I2C swons cons cons ]
	  [ ] ]
	cond
```

from Infix to ..


#### `I2T`

```joy
I2T ==
    unswons
	[ [ [unops in]
	    [I2T [] cons] dip swons ]
	  [ [[QUOTE] in]
	    [unswons [] cons] dip swons ]
	  [ [list]
	    I2T swap uncons swapd
	    I2T swons cons cons ]
	  [ [] cons ] ]
	cond
```


#### `P2C`

```joy
P2C ==
    unswons
	[ [ [unops in]
	    [P2C [] cons] dip swons ]
	  [ [binops in]
	    [P2C [] cons [P2C [] cons] dip swoncat]
	    dip swons ]
	  [ [list]
	    [] cons [QUOTE] swoncat ]
	  [ ] ]
	cond
```

from Polish to ..


#### `P2T`

```joy
P2T ==
    unswons
	[ [ [unops in]
	    [P2T [] cons] dip swons ]
	  [ [binops in]
	    [P2T [] cons [P2T [] cons] dip swoncat]
	    dip swons ]
	  [ [list]
	    [] cons [QUOTE] swoncat ]
	  [ [] cons ] ]
	cond
```


#### `P2I`

```joy
P2I ==
    unswons
	[ [ [unops in]
	    [P2I] dip swons ]
	  [ [binops in]
	    [P2I [P2I] dip swap]
	    dip swons concat [] cons ]
	  [ [list]
	    [QUOTE] swoncat ]
	  [ [] cons ] ]
	cond
```


#### `R2C`

```joy
R2C ==
    [ [ [ [unops in]
	      [[] cons] dip swons ]
	    [ [binops in]
	      [[] cons cons] dip swons ]
	    [ [list]
	      [] cons [QUOTE] swoncat ]
	    [ ] ]
	  cond ]
	step
```

from Reverse to ..


#### `R2T`

```joy
R2T ==
    [ [ [ [unops in]
	      [[] cons] dip swons ]
	    [ [binops in]
	      [[] cons cons] dip swons ]
	    [ [list]
	      [] cons [QUOTE] swoncat ]
	    [ [] cons ] ]
	  cond ]
	step
```


#### `R2I`

```joy
R2I ==
    [ [ [ [unops in]
	      swons ]
	    [ [binops in]
	      swons concat [] cons ]
	    [ [list]
	      [] cons [QUOTE] swoncat ]
	    [ [] cons ] ]
	  cond ]
	step
```


#### `X2Y`

```joy
X2Y ==
    unswons
	[ [ [unops in]
	    [X2Y] dip swap ]
	  [ [binops in]
	    [X2Y X2Y] dip swap ]
	  [ swap ] ]
	cond
```


#### `new-infra`

```joy
new-infra == cons [] swap infra
```


#### `M12T`

```joy
M12T ==
    M22T
	[ pop [null not] [first bin1ops in] sequand ]
	[ swap uncons swapd unswons M12T
	  rollupd [] cons cons cons ]
	[ ]
	ifte
```

Ideally the following should be inside the HIDE. But since currently    HIDE definitions cannot be mutually recursive, they are placed here.


#### `M22T`

```joy
M22T ==
    M32T
	[ pop [null not] [first bin2ops in] sequand ]
	[ swap uncons swapd unswons M22T
	  rollupd [] cons cons cons ]
	[ ]
	ifte
```


#### `M32T`

```joy
M32T ==
    M42T
	[ pop [null not] [first bin3ops in] sequand ]
	[ swap uncons swapd unswons M32T
	  rollupd [] cons cons cons ]
	[ ]
	ifte
```


#### `M42T`

```joy
M42T ==
    [ [ [ unops in ]
	    [ unswons M42T [] cons ] dip swons ]
	  [ [ [QUOTE] in ]
	    [ unswons [] cons ] dip swons ]
	  [ [ list]
	    unswons M12T popd ]
	  [ [] cons ] ]
	  cond
```


#### `M12C`

```joy
M12C ==
    M22C
	[ pop [null not] [first bin1ops in] sequand ]
	[ swap uncons swapd unswons M12C
	  rollupd [] cons cons cons ]
	[ ]
	ifte
```


#### `M22C`

```joy
M22C ==
    M32C
	[ pop [null not] [first bin2ops in] sequand ]
	[ swap uncons swapd unswons M22C
	  rollupd [] cons cons cons ]
	[ ]
	ifte
```


#### `M32C`

```joy
M32C ==
    M42C
	[ pop [null not] [first bin3ops in] sequand ]
	[ swap uncons swapd unswons M32C
	  rollupd [] cons cons cons ]
	[ ]
	ifte
```


#### `M42C`

```joy
M42C ==
    [ [ [ unops in ]
	    [ unswons M42C [] cons ] dip swons ]
	  [ [ [QUOTE] in ]
	    [ unswons [] cons ] dip swons ]
	  [ [ list]
	    unswons M12C popd ]
	  [ ] ]
	  cond
```


#### `Pol2Rev`

```joy
Pol2Rev == [X2Y] new-infra rest reverselist
```

In following, the fast reverselist is counted as 0.5 pass


#### `Pol2Cam`

```joy
Pol2Cam == P2C popd
```

1.5 passes


#### `Pol2Inf`

```joy
Pol2Inf == P2I popd
```

1   pass


#### `Pol2Tre`

```joy
Pol2Tre == P2T popd
```

1   pass


#### `Rev2Pol`

```joy
Rev2Pol == reverselist [X2Y] new-infra rest
```

1   pass


#### `Rev2Cam`

```joy
Rev2Cam == R2C
```

1.5 passes


#### `Rev2Inf`

```joy
Rev2Inf == R2I
```

1   pass


#### `Rev2Tre`

```joy
Rev2Tre == R2T
```

1   pass


#### `Cam2Pol`

```joy
Cam2Pol == [C2P] new-infra
```

1   pass


#### `Cam2Rev`

```joy
Cam2Rev == [C2R] new-infra
```

1   pass


#### `Cam2Tre`

```joy
Cam2Tre == C2T
```

1   pass


#### `Cam2Inf`

```joy
Cam2Inf == C2I
```

1   pass


#### `Inf2Pol`

```joy
Inf2Pol == Inf2Cam Cam2Pol
```

1   pass


#### `Inf2Rev`

```joy
Inf2Rev == Inf2Cam Cam2Rev
```

2   passes


#### `Inf2Cam`

```joy
Inf2Cam == I2C popd
```

2   passes


#### `Inf2Tre`

```joy
Inf2Tre == I2T popd
```

1   pass


#### `Min2Pol`

```joy
Min2Pol == Min2Cam Cam2Pol
```

1   pass


#### `Min2Rev`

```joy
Min2Rev == Min2Cam Cam2Rev
```

2   passes


#### `Min2Cam`

```joy
Min2Cam == unswons M12C popd
```

2   passes


#### `Min2Tre`

```joy
Min2Tre == unswons M12T popd
```

1   pass


#### `Min2Inf`

```joy
Min2Inf == Min2Cam Cam2Inf
```

1   pass


#### `j2f-f`

```joy
j2f-f ==
    [ list ]
        [ [[[]] concat] dip [j2f-f] step [[] cons concat] concat ]
        [ [] cons [concat] cons concat ]
        ifte
```


#### `j2f-r`

```joy
j2f-r ==
    [ list ]
        [ [[swons] swoncat] dip [j2f-r] step [] swons ]
        [ [] cons [swoncat] cons swoncat ]
        ifte
IN
    j2f-forwards == [[]] swap [j2f-f] step
```


#### `j2f-forwards`

```joy
j2f-forwards == [[]] swap [j2f-f] step
```


#### `j2f-reverse`

```joy
j2f-reverse == [] swap [j2f-r] step [] swons
```


### Other Stuff To Go Here

#### `SYMLIB`

```joy
SYMLIB == "symlib.joy - symbolic manipulation library\n"
```

other stuff to go here


---

## symtst.joy

### Definitions

#### `unops`

```joy
unops == [not succ pred fact fib first rest reverse i intern]
```


#### `binops`

```joy
binops == [and or + - * / = < > cons concat map filter]
```


#### `bin1ops`

```joy
bin1ops == [ = < > ]
```


#### `bin2ops`

```joy
bin2ops == [ + - or concat ]
```


#### `bin3ops`

```joy
bin3ops == [ * / and cons ]
```


---

## test.joy

---

## tutinp.joy

---

## tutlib.joy

### Definitions

#### `pause`

```joy
pause ==
    bell "\nTo return, hit  Control-" putchars control-eof putch
        newline terminal include
```


#### `pausing`

```joy
pausing == putchars pause
```


#### `praise`

```joy
praise ==
    [ "Good." "Splendid." "Excellent." "Great." "Terrific." ]
	                 0 at putchars newline
```

R.W.


#### `get-integer`

```joy
get-integer ==
    get
	[ integer not ]
	[ pop "An integer is expected:\n" putchars get ]
	while
```


#### `get-list`

```joy
get-list ==
    get
	[ list not ]
	[ pop "A list is expected:\n" putchars get ]
	while
```


#### `get-quote`

```joy
get-quote ==
    get
	[ list not ]
	[ pop "A quotation is expected:\n" putchars get ]
	while
```


#### `expecting`

```joy
expecting ==
    [ equal ]
	[ pop pop praise [succ] dip]
	[ "No, the right answer is:  " putchars put pop newline ]
	ifte
	succ
```


#### `expecting-from`

```joy
expecting-from ==
    [ [equal] some ]
	[ pop pop praise [succ] dip]
	[ "No, the right answer must be one of the following:\n\t"
          putchars [put "  " putchars] step pop newline ]
	ifte
	succ
```


#### `ini-stats`

```joy
ini-stats == 0 0
```


#### `put-stats`

```joy
put-stats ==
    "You answered " putchars [put] dip
	" of the " putchars put
	" questions correctly.\n\n" putchars
```


#### `toc-all-headings`

```joy
toc-all-headings ==
    toc-of-tutorial rest
	[space space second putchars newline] step
```


#### `toc-one-heading`

```joy
toc-one-heading == toc-of-tutorial of second putchars newline newline
```


#### `toc-do-section0`

```joy
toc-do-section0 ==
    newline
	0 toc-one-heading
	"Contents:\n" putchars toc-all-headings newline
	toc-of-tutorial first first i
```


#### `toc-ask-for-section`

```joy
toc-ask-for-section ==
    "\nTo repeat something, here are the sections again\n"
	putchars
	toc-all-headings
	[ "Type the number of the section you want, or \n"
	  "type  0  to do all sections, or\n"
	  "type any other number (even negative) to quit.\n" ]
	putstrings
	get-integer
	[toc-of-tutorial size >=] [0 swap -] [] ifte
```


#### `toc-do-section`

```joy
toc-do-section == toc-of-tutorial of first i
```


#### `toc-repeat-sections`

```joy
toc-repeat-sections ==
    toc-ask-for-section
	[ 0 >= ]
	[ toc-do-section
	  toc-ask-for-section ]
	while
```


#### `toc-do-sec0-loop`

```joy
toc-do-sec0-loop ==
    toc-do-section0
	toc-repeat-sections
	pop
	"Leaving  " putchars 0 toc-one-heading
	"\nBye\n\n" putchars
```


#### `TUTLIB`

```joy
TUTLIB == "tutlib.joy - tutorial library, for interaction"
```


---

## typlib.joy

### Definitions

#### `fatal2`

```joy
fatal2 == putchars putchars newline abort
```


#### `st_new`

```joy
st_new == []
```


#### `st_push`

```joy
st_push == swons
```


#### `st_null`

```joy
st_null == dup null
```


#### `st_top`

```joy
st_top == [null] ["st_top\n" _st_chk] [dup first] ifte
```


#### `st_pop`

```joy
st_pop == [null] ["st_pop\n" _st_chk] [rest] ifte
```


#### `st_pull`

```joy
st_pull == [null] ["st_pull\n" _st_chk] [unswons] ifte
```


#### `q_new`

```joy
q_new == [] []
```


#### `q_null`

```joy
q_null == _q_prep dup null
```


#### `q_add`

```joy
q_add == swap [swons] dip
```


#### `q_addl`

```joy
q_addl == swap [shunt] dip
```

add a list


#### `q_front`

```joy
q_front == _q_prep [null] ["q_front\n" _q_chk] [dup first] ifte
```

add a list


#### `q_rem`

```joy
q_rem == _q_prep [null] ["q_rem \n" _q_chk] [unswons] ifte
```


#### `t_new`

```joy
t_new == []
```


#### `t_reset`

```joy
t_reset == dup  unitlist unitlist
```


#### `t_add`

```joy
t_add == unitlist unitlist cons
```


#### `t_null`

```joy
t_null == _t_prep dup null
```


#### `t_front`

```joy
t_front ==
    _t_prep
                    [null] ["t_front\n" _t_chk] [dup first first] ifte
```


#### `t_rem`

```joy
t_rem ==
    _t_prep
                   [null]
                   ["t_rem\n" _t_chk]
                   [unswons unswons [swons] dip]
                   ifte
```


#### `bs_new`

```joy
bs_new == []
```

B&W p 230


#### `bs_union`

```joy
bs_union ==
    [ [ [null] [pop] ]
          [ [pop null] [swap pop] ]
          [ [unswons2 <] [[uncons] dip] [cons] ]
          [ [unswons2 >] [uncons swapd] [cons] ]
          [ [rest [uncons] dip] [cons] ] ]
        condlinrec
```


#### `bs_differ`

```joy
bs_differ ==
    [ [ [null] [pop]]
          [ [pop null] [pop pop []] ]
          [ [unswons2 <] [[uncons] dip] [cons] ]
          [ [unswons2 >] [rest] [] ]
          [ [[rest] dip rest] [] ] ]
        condlinrec
```


#### `bs_member`

```joy
bs_member ==
    [ [ [pop null] [pop2 false] ]
          [ [[first] dip >] [pop2 false] ]
          [ [[first] dip =] [pop2 true] ]
          [ [[rest] dip] [] ] ]
        condlinrec
```


#### `bs_insert`

```joy
bs_insert ==
    [ [ [pop null] [swons] ]
          [ [[first] dip >] [swons] ]
          [ [[first] dip =] [pop] ]
          [ [[uncons] dip] [cons] ] ]
        condlinrec
```


#### `bs_delete`

```joy
bs_delete ==
    [ [ [pop null] [pop] ]
          [ [[first] dip >] [pop] ]
          [ [[first] dip =] [pop rest] ]
          [ [[uncons] dip] [cons] ] ]
        condlinrec
```


### Old Recursive Versions

#### `r_bs_union`

```joy
r_bs_union ==
    [ [ [null]  pop ]
          [ [pop null]  swap pop ]
          [ [unswons2 <]  [uncons] dip r_bs_union cons ]
          [ [unswons2 >]  uncons  swapd r_bs_union cons ]
          [ rest [uncons] dip r_bs_union cons ] ]
        cond
```

old recursive versions


#### `r_bs_differ`

```joy
r_bs_differ ==
    [ [ [null]  pop ]
          [ [pop null]  pop pop [] ]
          [ [unswons2 <]  [uncons] dip r_bs_differ cons ]
          [ [unswons2 >]  rest r_bs_differ ]
          [ [rest] dip rest r_bs_differ ] ]
        cond
```


#### `r_bs_member`

```joy
r_bs_member ==
    [ [ [pop null]  pop2 false ]
          [ [[first] dip >]  pop2 false ]
          [ [[first] dip =]  pop2 true ]
          [ [rest] dip r_bs_member ] ]
        cond
```


#### `r_bs_insert1`

```joy
r_bs_insert1 == unitlist r_bs_union
```


#### `r_bs_delete1`

```joy
r_bs_delete1 == unitlist r_bs_differ
```


### End Old Recursive Versions

#### `d_new`

```joy
d_new == []
```


#### `d_null`

```joy
d_null == null
```


#### `d_add`

```joy
d_add ==
    [ [ [pop null] [swons] ]
          [ [[first] dip [first] app2 >=] [swons] ]
          [ [[uncons] dip] [cons] ] ]
        condlinrec
```


#### `d_union`

```joy
d_union ==
    [ [ [null] [pop] ]
          [ [pop null] [swap pop] ]
          [ [unswons2 [first] app2 <] [[uncons] dip] [cons] ]
          [ [unswons2 [first] app2 >] [uncons swapd] [cons] ]
          [ [uncons2] [cons cons] ] ]
        condlinrec
```


#### `d_differ`

```joy
d_differ ==
    [ [ [null] [pop]]
          [ [pop null] [pop pop []] ]
          [ [unswons2 [first] app2 <] [[uncons] dip] [cons] ]
          [ [unswons2 [first] app2 >] [rest] [] ]
          [ [[rest] dip rest] [] ] ]
        condlinrec
```


#### `d_look`

```joy
d_look ==
    [dup] dip
        [ [ [pop null] [pop pop "not found"] ]
          [ [[first first] dip >] [pop pop "not found"] ]
          [ [[first first] dip =] [pop first] ]
          [ [[rest] dip] [] ] ]
        condlinrec
```


#### `d_rem`

```joy
d_rem ==
    [ [ [pop null] [pop] ]
          [ [[first first] dip >] [pop] ]
          [ [[first first] dip =] [pop rest] ]
          [ [[uncons] dip] [cons] ] ]
        condlinrec
```


---

## usrlib.joy

### Definitions

#### `RAWJOY1`

```joy
RAWJOY1 == "the primitives of the Joy1 system\n"
```


#### `returned`

```joy
returned == "\007\nReturned to Joy\n" putchars
IN
    unix == true
```


#### `unix`

```joy
unix == true
```


#### `control-eof`

```joy
control-eof == 'D
```


#### `terminal`

```joy
terminal == "/dev/tty"
```


#### `ls`

```joy
ls == "ls -la" system
```


#### `editor`

```joy
editor == "vi "
```


#### `escape`

```joy
escape ==
    "\nTo return to Joy, type:   exit\n" putchars
	"csh" system
	returned
```


#### `edit`

```joy
edit ==
    dup editor swap concat system
	dup "Including  " putchars putchars '\n putch
	include
	returned
```


#### `find-in`

```joy
find-in ==
    [ [ [ [unix] first body null not ]
	    " " swap concat concat "grep " swap concat system ]
	  [ [ [vms] first body null not ]
	    swap " " swap concat concat "SEARCH " swap concat system ]
	  [ "unknown operating system for  find-in\n" putchars ] ]
 	cond
	returned
```


#### `standard-setting`

```joy
standard-setting == 1 setautoput 1 setundeferror
```


#### `USRLIB`

```joy
USRLIB == "usrlib.joy - (personal) user library\n"
```


#### `verbose`

```joy
verbose == true
```


---
