from z3 import *
import sys
import numpy as np 
import time

from cache_lookup import *
from tables import *

trace_file = sys.argv[1]

def grep(x, select=0):
  found = []
  with open(trace_file) as f:
    for line in f:
      parts = line.split(" ")
      if (parts[0] == x):
        found.append(int(parts[1], 16))
  return found[select] & ((1 << 6) - 1)


#define AES_FROUND(X0,X1,X2,X3,Y0,Y1,Y2,Y3)                     \
#    do                                                          \
#    {                                                           \
#        (X0) = *RK++ ^ AES_FT0( ( (Y0)       ) & 0xFF ) ^       \
#                       AES_FT1( ( (Y1) >>  8 ) & 0xFF ) ^       \
#                       AES_FT2( ( (Y2) >> 16 ) & 0xFF ) ^       \
#                       AES_FT3( ( (Y3) >> 24 ) & 0xFF );        \
#                                                                \
#        (X1) = *RK++ ^ AES_FT0( ( (Y1)       ) & 0xFF ) ^       \
#                       AES_FT1( ( (Y2) >>  8 ) & 0xFF ) ^       \
#                       AES_FT2( ( (Y3) >> 16 ) & 0xFF ) ^       \
#                       AES_FT3( ( (Y0) >> 24 ) & 0xFF );        \
#                                                                \
#        (X2) = *RK++ ^ AES_FT0( ( (Y2)       ) & 0xFF ) ^       \
#                       AES_FT1( ( (Y3) >>  8 ) & 0xFF ) ^       \
#                       AES_FT2( ( (Y0) >> 16 ) & 0xFF ) ^       \
#                       AES_FT3( ( (Y1) >> 24 ) & 0xFF );        \
#                                                                \
#        (X3) = *RK++ ^ AES_FT0( ( (Y3)       ) & 0xFF ) ^       \
#                       AES_FT1( ( (Y0) >>  8 ) & 0xFF ) ^       \
#                       AES_FT2( ( (Y1) >> 16 ) & 0xFF ) ^       \
#                       AES_FT3( ( (Y2) >> 24 ) & 0xFF );        \
#    } while( 0 )

ft0s = [
"R555555556256",
"R555555556282",
"R5555555562af",
"R5555555562cd",
"R555555556307",
"R55555555633a",
"R555555556368",
"R555555556376",
"R5555555563f8",
"R555555556429",
"R555555556460",
"R555555556480",
]
ft1s = [
"R55555555626c",
"R555555556296",
"R5555555562e2",
"R5555555562f9",
"R555555556320",
"R55555555634c",
"R55555555638a",
"R5555555563b6",
"R55555555640f",
"R55555555643d",
"R55555555647a",
"R5555555564a2",
]
ft2s = [
"R555555556278",
"R5555555562a5",
"R5555555562e9",
"R5555555562fd",
"R55555555632c",
"R55555555636c",
"R5555555563a1",
"R5555555563ba",
"R55555555641b",
"R55555555644a",
"R555555556493",
"R5555555564a9",
]
ft3s = [
"R55555555625d",
"R555555556286",
"R5555555562b3",
"R5555555562d4",
"R555555556315",
"R55555555633e",
"R55555555637a",
"R555555556396",
"R555555556404",
"R55555555642d",
"R555555556467",
"R555555556488",
]
##
guess_Y = [[[
  lookup_ft0[grep(ft0s[0], i)],
  lookup_ft1[grep(ft1s[3], i)],
  lookup_ft2[grep(ft2s[2], i)], 
  lookup_ft3[grep(ft3s[1], i)],
],
[
  lookup_ft0[grep(ft0s[1], i)],
  lookup_ft1[grep(ft1s[0], i)],
  lookup_ft2[grep(ft2s[3], i)],
  lookup_ft3[grep(ft3s[2], i)],
],
[
  lookup_ft0[grep(ft0s[2], i)],
  lookup_ft1[grep(ft1s[1], i)],
  lookup_ft2[grep(ft2s[0], i)],
  lookup_ft3[grep(ft3s[3], i)],
],
[
  lookup_ft0[grep(ft0s[3], i)],
  lookup_ft1[grep(ft1s[2], i)],
  lookup_ft2[grep(ft2s[1], i)],
  lookup_ft3[grep(ft3s[0], i)],
]] for i in range(4) ]

##
guess_X = [[[
  lookup_ft0[grep(ft0s[4], i)],
  lookup_ft1[grep(ft1s[7], i)],
  lookup_ft2[grep(ft2s[6], i)],
  lookup_ft3[grep(ft3s[5], i)],
],
[
  lookup_ft0[grep(ft0s[5], i)],
  lookup_ft1[grep(ft1s[4], i)],
  lookup_ft2[grep(ft2s[7], i)],
  lookup_ft3[grep(ft3s[6], i)],
],
[
  lookup_ft0[grep(ft0s[7], i)],
  lookup_ft1[grep(ft1s[5], i)],
  lookup_ft2[grep(ft2s[4], i)],
  lookup_ft3[grep(ft3s[7], i)],
],
[
  lookup_ft0[grep(ft0s[6], i)],
  lookup_ft1[grep(ft1s[6], i)],
  lookup_ft2[grep(ft2s[5], i)],
  lookup_ft3[grep(ft3s[4], i)],
]] for i in range(4)]

# add last y
guess_Y.append([[
  lookup_ft0[grep(ft0s[ 8], 0)],
  lookup_ft1[grep(ft1s[11], 0)],
  lookup_ft2[grep(ft2s[10], 0)],
  lookup_ft3[grep(ft3s[ 9], 0)],
],
[
  lookup_ft0[grep(ft0s[ 9], 0)],
  lookup_ft1[grep(ft1s[ 8], 0)],
  lookup_ft2[grep(ft2s[11], 0)],
  lookup_ft3[grep(ft3s[10], 0)],
],
[
  lookup_ft0[grep(ft0s[10], 0)],
  lookup_ft1[grep(ft1s[ 9], 0)],
  lookup_ft2[grep(ft2s[ 8], 0)],
  lookup_ft3[grep(ft3s[11], 0)],
],
[
  lookup_ft0[grep(ft0s[11], 0)],
  lookup_ft1[grep(ft1s[10], 0)],
  lookup_ft2[grep(ft2s[ 9], 0)],
  lookup_ft3[grep(ft3s[ 8], 0)],
]])

guess_rks = {}


number_rk_rounds = 10

for i in range(number_rk_rounds):
  guess_rks[3+4*i] = [
    lookup_rk[grep("R555555555cfd", i)],
    lookup_rk[grep("R555555555cdf", i)],
    lookup_rk[grep("R555555555d14", i)],
    lookup_rk[grep("R555555555ced", i)],
  ]

#key = [BitVec(f"key_{i}", 8) for i in range(16)]
pt  = [BitVec(f"pt_{i}", 8) for i in range(16)]

known_pt = []
if len(sys.argv) == 3:
  for i in range(0, len(sys.argv[2]), 2):
    known_pt.append( int(sys.argv[2][i:i+2], 16) )

#print(f"using known pt: {[hex(x) for x in known_pt]}")

for i, kpt in enumerate(known_pt[::-1]):
  s.add( pt[15-i] == kpt )
  
n_Y = 2
n_X = 1

Y = [[[BitVec(f"Y{j}_{x}_{i}", 8) for i in range(4)] for x in range(4)] for j in range(n_Y)]
X = [[[BitVec(f"X{j}_{x}_{i}", 8) for i in range(4)] for x in range(4)] for j in range(n_X)]

### this is the data from the key derivation

# we know that the first round keys are the key bytes
rks = []
rks.append( [ Y[0][0][b] ^ pt[ 0+b] for b in range(4) ] )
rks.append( [ Y[0][1][b] ^ pt[ 4+b] for b in range(4) ] )
rks.append( [ Y[0][2][b] ^ pt[ 8+b] for b in range(4) ] )
rks.append( [ Y[0][3][b] ^ pt[12+b] for b in range(4) ] )

# add the key derivation
for r in range(number_rk_rounds):
  rk0 = rks[r*4+0]
  rk1 = rks[r*4+1]
  rk2 = rks[r*4+2]
  rk3 = rks[r*4+3]

  rk4 = [ rk0[b] ^ get_rcon_byte(r, b) ^ FSb[rk3[shift_index[b]]] for b in range(4)  ] 
  rk5 = [ rk4[b] ^ rk1[b] for b in range(4) ]
  rk6 = [ rk5[b] ^ rk2[b] for b in range(4) ]
  rk7 = [ rk6[b] ^ rk3[b] for b in range(4) ]
  
  rks.append(rk4)
  rks.append(rk5)
  rks.append(rk6)
  rks.append(rk7)

# add the bounds for the rks
for rk in range(number_rk_rounds):
  for i in range(4):
    s.add( And( ULE( guess_rks[3+rk*4][i][0], rks[3+rk*4][i] ), ULE( rks[3+rk*4][i], guess_rks[3+rk*4][i][1] ) ) )

### this is the data from the encryption

# we also know that y0{0-3} are guesses for the key bytes
#for i in range(4): # variable index
#  for b in range(4): # byte index
#    s.add( Y[0][i][b] == key[i*4+b] ^ pt[i*4+b])
  
# add the leaked bounds

for j in range(n_Y): # iteration index
  for i in range(4): # variable index
    for b in range(4): # byte index
      s.add( And( ULE( guess_Y[j][i][b][0], Y[j][i][b] ), ULE( Y[j][i][b], guess_Y[j][i][b][1] ) ) )

for j in range(n_X): # iteration index
  for i in range(4): # variable index
    for b in range(4): # byte index
      s.add( And( ULE( guess_X[j][i][b][0], X[j][i][b] ), ULE( X[j][i][b], guess_X[j][i][b][1] ) ) )

# the aes fround
def aes_fround(s, YY, XX, ki):
  for i in range(4): # variable index
    for b in range(4): # byte index
      i0 = (i+0) % 4
      i1 = (i+1) % 4
      i2 = (i+2) % 4
      i3 = (i+3) % 4
      s.add( YY[i][b] == rks[i+ki][b] ^ FT0x[b][XX[i0][0]] ^ FT1x[b][XX[i1][1]] ^ FT2x[b][XX[i2][2]] ^ FT3x[b][XX[i3][3]] )

aes_fround(s, X[0], Y[0],  4)

# the more rounds added the fewer candidates but the longer it takes to solve

#aes_fround(s, Y[1], X[0],  8)
#aes_fround(s, X[1], Y[1], 12)

#aes_fround(s, Y[2], X[1], 16)
#aes_fround(s, X[2], Y[2], 20)

#aes_fround(s, Y[3], X[2], 24)
#aes_fround(s, X[3], Y[3], 28)

#aes_fround(s, Y[4], X[3], 32)



## solve!

counter = 0
#set_option(max_args=10000000, max_lines=1000000, max_depth=10000000, max_visited=1000000)
#print(s.assertions())


with open("out.smt", "w") as f:
  f.write("(set-logic QF_BV)\n")
  f.write(s.sexpr())
  f.write("(check-sat)")

start = time.time()

while s.check() != unsat:
  m = s.model()
  sys.stdout.flush()

  block = []

  print(f"candidate {counter:3d}: ", end='')

  s.push()

  aes_fround(s, Y[1], X[0],  8)

  for i in range(4):
    for b in range(4):
      y = m[Y[0][i][b]].as_long()
      p = m[pt[i*4+b]].as_long()
      v = y ^ p
      
      print(f"{v:02x}", end='')
      block.append( v != Y[0][i][b] ^ pt[i*4+b])
      #block.append( y != Y[0][i][b])
      block.append( p != pt[i*4+b] )

      s.add( y == Y[0][i][b])
      s.add( p == pt[i*4+b])

  correct = s.check() != unsat

  s.pop()

  print(" pt: ", end='')
  for i in range(16):
    v = m[pt[i]].as_long()
    print(f"{v:02x}", end='')

  if correct:
    print(" <- correct")
    break
  else:
    print("\r", end='')
    
  counter += 1
  s.add(Or(block))

end = time.time()

print("")
print(f"took {int((end - start)//60):4d}:{int((end - start)%60):02d} m:s")