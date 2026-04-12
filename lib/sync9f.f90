subroutine sync9f(s2,nq,nfa,nfb,ss2,ss3,lagpk,ipk,ccfbest)

! Look for JT9 sync pattern in the folded symbol spectra, s2.
! Frequency search extends from nfa to nfb.  Synchronized symbol
! spectra are put into ss2() and ss3().

  integer ii4(16)
  real s2(240,340)
  real ss2(0:8,85)
  real ss3(0:7,69)
  include 'jt9sync.f90'

  ii4=4*ii-3
  ccf=0.
  ccfbest=0.
  nfft=4*nq
  df=12000.0/nfft
  ia=nfa/df
  ib=nfb/df + 0.9999

  do i=ia,ib
     do lag=0,339
        t=0.
        do n=1,16
           j=ii4(n)+lag
           if(j.gt.340) j=j-340
           t=t + s2(i,j)
        enddo
        if(t.gt.ccfbest) then
           lagpk=lag
           ipk=i
           ccfbest=t
        endif
     enddo
  enddo

  do i=0,8
     j4=lagpk-4
     i2=2*i + ipk
     if(i2.lt.1) i2=1
     m=0
     do j=1,85
        j4=j4+4
        if(j4.gt.340) j4=j4-340
        if(j4.lt.1) j4=j4+340
        ss2(i,j)=s2(i2,j4)
        if(i.ge.1 .and. isync(j).eq.0) then
           m=m+1
           ss3(i-1,m)=ss2(i,j)
        endif
     enddo
  enddo

  return
end subroutine sync9f
