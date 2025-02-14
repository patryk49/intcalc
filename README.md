# simple interpreter

## parsing benchmark on 1M lines on fib functions:

```
token count    :   8960001
ast node count :   6720000
node/token count ratio : 0.750000

tokens size    :  12992002
ast nodes size :  11088001
nodes/tokens size ratio : 0.853448

lexing speed     :     70117235.06 [tokens/s]
parsing speed    :    215005599.10 [nodes/s]
making ast speed :     42248473.84 [nodes/s]

reading time    :0.000018 [s]
lexing time     :0.127786 [s]
parsing time    :0.031255 [s]
making ast time :0.159059 [s]


reading speed    : 1424888.89 [MB/s]
lexing speed     :     200.71 [MB/s]
parsing speed    :     820.60 [MB/s]
making ast speed :     161.25 [MB/s]
```
