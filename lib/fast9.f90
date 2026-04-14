module fast9_contexts
  implicit none

  integer, parameter :: FAST9_NSAVE=500
  integer, parameter :: FAST9_S1_SIZE=720000

  type fast9_state
     logical :: initialized=.false.
     integer :: nsubmode0=-1
     integer :: ntot=0
     real :: s1(FAST9_S1_SIZE)
  end type fast9_state

  type(fast9_state), save, target :: shared_fast9_state

contains

  subroutine init_fast9_state(state)
    type(fast9_state), intent(inout) :: state

    if(state%initialized) return

    state%s1=0.0
    state%nsubmode0=-1
    state%ntot=0
    state%initialized=.true.
  end subroutine init_fast9_state

end module fast9_contexts

subroutine fast9(id2,narg,line)

! Decoder for "fast9" modes, JT9E to JT9H.

  use fast9_contexts, only: fast9_state, shared_fast9_state, init_fast9_state,  &
       FAST9_NSAVE

  parameter (NMAX=30*12000)
  integer*2 id2(0:NMAX)
  integer narg(0:14)
  integer*1 i1SoftSymbols(207)
  integer*1, allocatable :: i1save(:,:)
  integer indx(FAST9_NSAVE)
  integer*8 count0,count1,clkfreq
  real, allocatable :: s2(:,:)         !Symbol spectra at quarter-symbol steps
  real ss2(0:8,85)                     !Folded symbol spectra
  real ss3(0:7,69)                     !Folded spectra without sync symbols
  real s(1500)
  real ccfsave(FAST9_NSAVE)
  real t0save(FAST9_NSAVE)
  real t1save(FAST9_NSAVE)
  real freqSave(FAST9_NSAVE)
  real t(6)
  character*22 msg                     !Decoded message
  character*80 line(100)
  type(fast9_state), pointer :: state

  state => shared_fast9_state
  call init_fast9_state(state)

! Parameters from GUI are in narg():
  nutc=narg(0)                         !UTC
  npts=min(narg(1),NMAX)               !Number of samples in id2 (12000 Hz)
  nsubmode=narg(2)                     !0=A 1=B 2=C 3=D 4=E 5=F 6=G 7=H
  if(nsubmode.lt.4) go to 900
  newdat=narg(3)                       !1==> new data, compute symbol spectra
  minsync=narg(4)                      !Lower sync limit
  npick=narg(5)
  t0=0.001*narg(6)
  t1=0.001*narg(7)
  maxlines=narg(8)                     !Max # of decodes to return to caller
  nmode=narg(9)
  nrxfreq=narg(10)                     !Targer Rx audio frequency (Hz)
  ntol=narg(11)                        !Search range, +/- ntol (Hz)

  tmid=npts*0.5/12000.0
  line(1:100)(1:1)=char(0)
  s=0
  allocate(i1save(207,FAST9_NSAVE))
  allocate(s2(240,340))
  i1save=0
  s2=0
  nsps=60 * 2**(7-nsubmode)            !Samples per sysbol
  nfft=2*nsps                          !FFT size
  nh=nfft/2
  nq=nfft/4
  istep=nsps/4                         !Symbol spectra at quarter-symbol steps
  jz=npts/istep
  df=12000.0/nfft                      !FFT bin width
  db1=db(2500.0/df)
  nfa=max(200,nrxfreq-ntol)            !Lower frequency limit
  nfb=min(nrxfreq+ntol,2500)           !Upper frequency limit
  nline=0
  t=0.

  if(newdat.eq.1 .or. nsubmode.ne.state%nsubmode0) then
     call system_clock(count0,clkfreq)
     call spec9f(id2,npts,nsps,state%s1,jz,nq)     !Compute symbol spectra, s1
     call system_clock(count1,clkfreq)
     t(1)=t(1)+float(count1-count0)/float(clkfreq)
  endif

  state%nsubmode0=nsubmode
  tmsg=nsps*85.0/12000.0
  limit=2000
  nlen0=0
  i1=0
  i2=0
  ccfsave=0.
  do ilength=1,14
     nlen=1.4142136**(ilength-1)
     if(nlen.gt.jz/340) nlen=jz/340
     if(nlen.eq.nlen0) cycle
     nlen0=nlen

     db0=db(float(nlen))
     jlen=nlen*340
     jstep=jlen/4                      !### Is this about right? ###
     if(nsubmode.ge.6) jstep=jlen/2

     do ja=1,jz-jlen,jstep
        jb=ja+jlen-1
        call system_clock(count0,clkfreq)
        call foldspec9f(state%s1,nq,jz,ja,jb,s2)   !Fold symbol spectra into s2
        call system_clock(count1,clkfreq)
        t(2)=t(2)+float(count1-count0)/float(clkfreq)

! Find sync; put sync'ed symbol spectra into ss2 and ss3
! Might want to do a peakup in DT and DF, then re-compute symbol spectra.

        call system_clock(count0,clkfreq)
        call sync9f(s2,nq,nfa,nfb,ss2,ss3,lagpk,ipk,ccfbest)
        call system_clock(count1,clkfreq)
        t(3)=t(3)+float(count1-count0)/float(clkfreq)

        i1=i1+1
        if(ccfbest.lt.30.0) cycle
        call system_clock(count0,clkfreq)
        call softsym9f(ss2,ss3,i1SoftSymbols)     !Compute soft symbols
        call system_clock(count1,clkfreq)
        t(4)=t(4)+float(count1-count0)/float(clkfreq)

        i2=i2+1
        ccfsave(i2)=ccfbest
        i1save(1:207,i2)=i1SoftSymbols
        t0=(ja-1)*istep/12000.0
        t1=(jb-1)*istep/12000.0
        t0save(i2)=t0
        t1save(i2)=t1
        freq=ipk*df
        freqSave(i2)=freq
     enddo
  enddo
  nsaved=i2

  ccfsave(1:nsaved)=-ccfsave(1:nsaved)
  call system_clock(count0,clkfreq)
  indx=0
  call indexx(ccfsave,nsaved,indx)
  call system_clock(count1,clkfreq)
  t(5)=t(5)+float(count1-count0)/float(clkfreq)

  ccfsave(1:nsaved)=-ccfsave(1:nsaved)

  do iter=1,2
     do isave=1,50
        i2=indx(isave)
        if(i2.lt.1 .or. i2.gt.nsaved) cycle      !### Why needed? ###
        t0=t0save(i2)
        t1=t1save(i2)
        if(iter.eq.1 .and. t1.lt.tmid) cycle
        if(iter.eq.2 .and. t1.ge.tmid) cycle
        ccfbest=ccfsave(i2)
        i1SoftSymbols=i1save(1:207,i2)
        freq=freqSave(i2)
        call system_clock(count0,clkfreq)
        call jt9fano(i1SoftSymbols,limit,nlim,msg)      !Invoke Fano decoder
        call system_clock(count1,clkfreq)
        t(6)=t(6)+float(count1-count0)/float(clkfreq)

        i=t0*12000.0
        kz=(t1-t0)/0.02
        smax=0.
        do k=1,kz
           sq=0.
           do n=1,240
              i=i+1
              x=id2(i)
              sq=sq+x*x
           enddo
           s(k)=sq/240.
           smax=max(s(k),smax)
        enddo
        call pctile(s,kz,35,base)
        snr=smax/(1.1*base) - 1.0
        nsnr=-20
        if(snr.gt.0.0) nsnr=nint(db(snr))

        if(msg.ne.'                      ') then

! Display multiple decodes only if they differ:
           do n=1,nline
              if(index(line(n),msg).gt.1) go to 100
           enddo
!### Might want to use decoded message to get a complete estimate of S/N.
           nline=nline+1
           write(line(nline),1000) nutc,nsnr,t0,nint(freq),msg,char(0)
1000       format(i6.6,i4,f5.1,i5,1x,'@ ',1x,a22,a1)
           state%ntot=state%ntot+1
           if(nline.ge.maxlines) go to 900
        endif
100     continue
     enddo
  enddo

900 continue
  if(allocated(i1save)) deallocate(i1save)
  if(allocated(s2)) deallocate(s2)

  return
end subroutine fast9
