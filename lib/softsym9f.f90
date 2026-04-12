subroutine softsym9f(ss2,ss3,i1SoftSymbols)

! Compute soft symbols and S/N

  real ss2(0:8,85)
  real ss3(0:7,69)
  integer*1 i1SoftSymbolsScrambled(207)
  integer*1 i1SoftSymbols(207)

  ss=0.
  sig=0.
  if(ss2(0,1).eq.-999.0) return       !Silence compiler warning
  do j=1,69
     smax=0.
     do i=0,7
        smax=max(smax,ss3(i,j))
        ss=ss+ss3(i,j)
     enddo
     sig=sig+smax
     ss=ss-smax
  enddo
  ave=ss/(69*7)                       !Baseline
!  call pctile(ss2,9*85,35,xmed)                       !### better? ###
  ss3=ss3/ave
  sig=sig/69.                         !Signal

  m0=3
  k=0
  scale=10.0
  do j=1,69
     do m=m0-1,0,-1                   !Get bit-wise soft symbols
        if(m.eq.2) then
           r1=max(ss3(4,j),ss3(5,j),ss3(6,j),ss3(7,j))
           r0=max(ss3(0,j),ss3(1,j),ss3(2,j),ss3(3,j))
        else if(m.eq.1) then
           r1=max(ss3(2,j),ss3(3,j),ss3(4,j),ss3(5,j))
           r0=max(ss3(0,j),ss3(1,j),ss3(6,j),ss3(7,j))
        else
           r1=max(ss3(1,j),ss3(2,j),ss3(4,j),ss3(7,j))
           r0=max(ss3(0,j),ss3(3,j),ss3(5,j),ss3(6,j))
        endif

        k=k+1
        i4=nint(scale*(r1-r0))
        if(i4.lt.-127) i4=-127
        if(i4.gt.127) i4=127
        i1SoftSymbolsScrambled(k)=i4
     enddo
  enddo


  call interleave9(i1SoftSymbolsScrambled,-1,i1SoftSymbols)

  return
end subroutine softsym9f
