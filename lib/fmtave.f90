program fmtave

! Average groups of frequency-calibration measurements.
  
  implicit real*8 (a-h,o-z)
  character infile*80
  character*8 cutc,cutc1

  nargs=iargc()
  if(nargs.ne.1) then
     print*,'Usage:   fmtave <infile>'
     print*,'Example: fmtave fmt.all'
     go to 999
  endif
  call getarg(1,infile)

  open(10,file=infile,status='old')
  open(12,file='fmtave.out',status='unknown')

  write(*,1000)
1000 format('    Freq     DF    CAL   N     rms    UTC     Call'/       &
            '   (kHz)    (Hz)    ?         (Hz)'/                       &
            '----------------------------------------------------')
  nkhz0=0
  sum=0.d0
  sumsq=0.d0
  n=0
  do i=1,99999
     read(10,*,end=10) cutc,nkHz,ncal,noffset,faudio,df,dblevel,snr
     if((nkHz.ne.nkHz0) .and. i.ne.1) then
        ave=sum/n
        rms=0.d0
        if(n.gt.1) then
           rms=sqrt(abs(sumsq - sum*sum/n)/(n-1.d0))
        endif
        fMHz=0.001d0*nkHz0
        write(*,1010)  fMHz,ave,ncal0,n,rms,cutc1
        write(12,1010) fMHz,ave,ncal0,n,rms,cutc1
1010    format(f8.3,f9.3,i4,i5,f8.2,2x,a8,2x,a6)
        sum=0.d0
        sumsq=0.d0
        n=0
     endif
     dial_error=faudio-noffset
     sum=sum + dial_error
     sumsq=sumsq + dial_error**2
     n=n+1
     if(n.eq.1) then
        cutc1=cutc
        ncal0=ncal
     endif
     nkHz0=nkHz
  enddo

10 ave=sum/n
  rms=0.d0
  if(n.gt.0) then
     rms=sqrt((sumsq - sum*sum/n)/(n-1.d0))
  endif
  fMHz=0.001d0*nkHz
  write(*,1010)  fMHz,ave,ncal,n,rms,cutc1
  write(12,1010) fMHz,ave,ncal,n,rms,cutc1

999 end program fmtave
