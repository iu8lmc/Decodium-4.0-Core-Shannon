subroutine softsym9w(id2,npts,xdt0,f0,width,nsubmode,xdt1,snrdb,i1softsymbols)

  parameter (NFFT=6912,NH=NFFT/2,NQ=NH/2)
  real s(NQ)
  real s2(0:8,85)
  real s3(0:7,69)
  real x(NFFT)
  complex cx(0:NH)
  integer*2 id2(60*12000)
  integer*1 i1SoftSymbolsScrambled(207)
  integer*1 i1softsymbols(207)
  include 'jt9sync.f90'
  equivalence (x,cx)

  if(npts.eq.-99) stop                     !Silence compiler warning
  df=12000.0/NFFT
  i0a=max(1.0,(xdt0-1.0)*12000.0)
  i0b=(xdt0+1.0)*12000.0
  k1=max(1,nint((f0-0.5*width)/df))
  k2=min(NQ,nint((f0+0.5*width)/df))
  smax=0.
  i0pk=1
  i1softsymbols=0

  do i0=i0a,i0b,432
     s=0.
     ssum=0.
     do j=1,16
        ia=i0 + (ii(j)-1)*nfft
        ib=ia+NFFT-1
        x=1.e-6*id2(ia:ib)
        call four2a(x,nfft,1,-1,0)        !r2c FFT
        do k=1,NQ
           s(k)=s(k) + real(cx(k))**2 + aimag(cx(k))**2
        enddo
     enddo
     ssum=ssum + sum(s(k1:k2))
     if(ssum.gt.smax) then
        smax=ssum
        i0pk=i0
     else 
        if(ssum.lt.0.7*smax) exit
     endif
  end do
  xdt1=(i0pk-1)/12000.0

  if(i0pk.le.0) go to 999

  m=0
  do j=1,85
     ia=i0pk + (j-1)*nfft
     ib=ia+NFFT-1
     x=1.e-6*id2(ia:ib)
     call four2a(x,nfft,1,-1,0)        !r2c FFT
     do k=1,NQ
        s(k)=real(cx(k))**2 + aimag(cx(k))**2
     enddo

     dtone=df*(2**nsubmode)
     do i=0,8
        f=f0 + i*dtone
        k1=max(1,nint((f-0.5*width)/df))
        k2=min(NQ,nint((f+0.5*width)/df))
        s2(i,j)=sum(s(k1:k2))                  !Symbol spectra, including sync
     enddo

     if(isync(j).eq.0) then
        m=m+1
        s3(0:7,m)=s2(1:8,j)                   !Symbol spectra, data only
     endif

!     write(19,3101) j,s2(0:8,j)
!3101 format(i2,9f8.2)
  enddo

  ss=0.
  sig=0.
  do j=1,69
     smax=0.
     do i=0,7
        smax=max(smax,s3(i,j))
        ss=ss+s3(i,j)
     enddo
     sig=sig+smax
     ss=ss-smax
  enddo
  ave=ss/(69*7)                           !Baseline
  call pctile(s2,9*85,35,xmed)
  s3=s3/ave
  sig=sig/69.                             !Signal
  snrdb=db(sig/xmed) - 28.0
     
  m0=3
  k=0
  do j=1,69
     smax=0.
     do i=0,7
        if(s3(i,j).gt.smax) smax=s3(i,j)
     enddo
 
     do m=m0-1,0,-1                   !Get bit-wise soft symbols
        if(m.eq.2) then
           r1=max(s3(4,j),s3(5,j),s3(6,j),s3(7,j))
           r0=max(s3(0,j),s3(1,j),s3(2,j),s3(3,j))
        else if(m.eq.1) then
           r1=max(s3(2,j),s3(3,j),s3(4,j),s3(5,j))
           r0=max(s3(0,j),s3(1,j),s3(6,j),s3(7,j))
        else
           r1=max(s3(1,j),s3(2,j),s3(4,j),s3(7,j))
           r0=max(s3(0,j),s3(3,j),s3(5,j),s3(6,j))
        endif

        k=k+1
        i4=nint(10.0*(r1-r0))
        if(i4.lt.-127) i4=-127
        if(i4.gt.127) i4=127
        i1SoftSymbolsScrambled(k)=i4
     enddo
  enddo

! Remove interleaving
  call interleave9(i1SoftSymbolsScrambled,-1,i1SoftSymbols)

999  return
end subroutine softsym9w
