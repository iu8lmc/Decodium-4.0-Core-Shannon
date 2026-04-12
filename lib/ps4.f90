subroutine ps4(dat,nfft,s)

  parameter (NMAX=2520+2)
  parameter (NHMAX=NMAX/2-1)
  real dat(nfft)
  real dat2(NMAX)
  real s(NHMAX)
  complex c(0:NMAX)
  equivalence(dat2,c)

  nh=nfft/2
  do i=1,nh
     dat2(i)=dat(i)/128.0       !### Why 128 ??
  enddo
  do i=nh+1,nfft
     dat2(i)=0.
  enddo

  call four2a(c,nfft,1,-1,0)

  fac=1.0/nfft
  do i=1,nh
     s(i)=fac*(real(c(i))**2 + aimag(c(i))**2)
  enddo

  return
end subroutine ps4
