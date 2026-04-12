subroutine foldspec9f(s1,nq,jz,ja,jb,s2)

! Fold symbol spectra (quarter-symbol steps) from s1 into s2

  real s1(nq,jz)
  real s2(240,340)                       !340 = 4*85
  integer nsum(340)

  s2=0.
  nsum=0

  do j=ja,jb
     k=mod(j-1,340)+1
     nsum(k)=nsum(k)+1
     do i=1,NQ
        s2(i,k)=s2(i,k) + s1(i,j)
     enddo
  enddo

  do k=1,340
     fac=1.0
     if(nsum(k).gt.0) fac=1.0/nsum(k)
     s2(1:nq,k)=fac*s2(1:nq,k)
  enddo

  ave=sum(s2)/(340.0*nq)
  if(ave.gt.0.0) s2=s2/ave

  return
end subroutine foldspec9f
