module ft2_decode

! Multi-period averaging buffers (module-level, persist across calls)
   integer, parameter :: NN_AVG=103     !NN from ft2_params: NS(16)+ND(87)
   real, save :: bm_avg(2*NN_AVG,3)    !Accumulated bitmetrics (EMA)
   integer, save :: navg_ft2 = 0       !Number of periods accumulated
   real, save :: f_avg = 0.0           !Frequency of averaged signal
   real, save :: dt_avg = 0.0          !DT of averaged signal

   type :: ft2_decoder
      procedure(ft2_decode_callback), pointer :: callback
   contains
      procedure :: decode
   end type ft2_decoder

   abstract interface
      subroutine ft2_decode_callback (this,sync,snr,dt,freq,decoded,nap,qual)
         import ft2_decoder
         implicit none
         class(ft2_decoder), intent(inout) :: this
         real, intent(in) :: sync
         integer, intent(in) :: snr
         real, intent(in) :: dt
         real, intent(in) :: freq
         character(len=37), intent(in) :: decoded
         integer, intent(in) :: nap
         real, intent(in) :: qual
      end subroutine ft2_decode_callback
   end interface

contains

   subroutine decode(this,callback,iwave,nQSOProgress,nfqso,    &
      nfa,nfb,ndepth,lapcqonly,ncontest,mycall,hiscall)
      use timer_module, only: timer
      use packjt77
      include 'ft2/ft2_params.f90'
      parameter (MAXCAND=300)
      class(ft2_decoder), intent(inout) :: this
      procedure(ft2_decode_callback) :: callback
      parameter (NSS=NSPS/NDOWN,NDMAX=NMAX/NDOWN)
      character message*37,msgsent*37
      character c77*77
      character*37 decodes(100)
      character*17 cdatetime0
      character*12 mycall,hiscall
      character*12 mycall0,hiscall0
      character*6 hhmmss

      complex cd2(0:NDMAX-1)                  !Complex waveform
      complex cb(0:NDMAX-1)
      complex cd(0:NN*NSS-1)                       !Complex waveform
      complex ctwk(2*NSS),ctwk2(2*NSS,-16:16)

      real a(5)
      real bitmetrics(2*NN,3)
      real dd(NMAX)
      real llr(2*ND),llra(2*ND),llrb(2*ND),llrc(2*ND),llrd(2*ND),llre(2*ND)
      real candidate(2,MAXCAND)
      real savg(NH1),sbase(NH1)

      integer apbits(2*ND)
      integer apmy_ru(28),aphis_fd(28)
      integer*2 iwave(NMAX)                 !Raw received data
      integer*1 message77(77),rvec(77),apmask(2*ND),cw(2*ND)
      integer*1 message91(91)
      integer*1 hbits(2*NN)
      integer i4tone(103)
      integer nappasses(0:5)    ! # of decoding passes for QSO States 0-5
      integer naptypes(0:5,4)   ! nQSOProgress, decoding pass
      integer mcq(29),mcqru(29),mcqfd(29),mcqtest(29),mcqww(29)
      integer mrrr(19),m73(19),mrr73(19)
      integer ndepth0                      !Depth bits only (0-7)

! Multi-period averaging tracking (per-period, not saved)
      real best_bm(2*NN,3)                 !Best bitmetrics near nfqso this period
      real best_f1_avg                     !Freq of best candidate this period
      real best_sync_avg                   !Best sync near nfqso this period
      integer best_ibest_avg               !DT of best candidate this period
      logical got_candidate                !Found good candidate near nfqso?

      logical nohiscall,unpk77_success
      logical first, dobigfft
      logical dosubtract,doosd
      logical badsync
      logical, intent(in) :: lapcqonly

      data first/.true./
      data     mcq/0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0/
      data   mcqru/0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,1,1,1,0,0,1,1,0,0/
      data   mcqfd/0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,1,0/
      data mcqtest/0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,1,0,1,0,1,1,1,1,1,1,0,0,1,0/
      data   mcqww/0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,1,1,1,1,0/
      data    mrrr/0,1,1,1,1,1,1,0,1,0,0,1,0,0,1,0,0,0,1/
      data     m73/0,1,1,1,1,1,1,0,1,0,0,1,0,1,0,0,0,0,1/
      data   mrr73/0,1,1,1,1,1,1,0,0,1,1,1,0,1,0,1,0,0,1/
      data rvec/0,1,0,0,1,0,1,0,0,1,0,1,1,1,1,0,1,0,0,0,1,0,0,1,1,0,1,1,0, &
         1,0,0,1,0,1,1,0,0,0,0,1,0,0,0,1,0,1,0,0,1,1,1,1,0,0,1,0,1, &
         0,1,0,1,0,1,1,0,1,1,1,1,1,0,0,0,1,0,1/
      save fs,dt,tt,txt,twopi,h,first,apbits,nappasses,naptypes, &
         mycall0,hiscall0,ctwk2

      this%callback => callback
      hhmmss=cdatetime0(8:13)
      dxcall13=hiscall        ! initialize for use in packjt77
      mycall13=mycall

      smax1=0.
      nd1=0

      if(first) then
         fs=12000.0/NDOWN                !Sample rate after downsampling
         dt=1/fs                         !Sample interval after downsample (s)
         tt=NSPS*dt                      !Duration of "itone" symbols (s)
         txt=NZ*dt                       !Transmission length (s) without ramp up/down
         twopi=8.0*atan(1.0)
         h=1.0

         do idf=-16,16
            a=0.
            a(1)=real(idf)
            ctwk=1.
            call twkfreq1(ctwk,2*NSS,fs/2.0,a,ctwk2(:,idf))
         enddo

         mcq=2*mod(mcq+rvec(1:29),2)-1
         mcqru=2*mod(mcqru+rvec(1:29),2)-1
         mcqfd=2*mod(mcqfd+rvec(1:29),2)-1
         mcqtest=2*mod(mcqtest+rvec(1:29),2)-1
         mcqww=2*mod(mcqww+rvec(1:29),2)-1
         mrrr=2*mod(mrrr+rvec(59:77),2)-1
         m73=2*mod(m73+rvec(59:77),2)-1
         mrr73=2*mod(mrr73+rvec(59:77),2)-1
         nappasses(0)=3
         nappasses(1)=3
         nappasses(2)=3
         nappasses(3)=4
         nappasses(4)=4
         nappasses(5)=4

! iaptype
!------------------------
!   1        CQ     ???    ???           (29 ap bits)
!   2        MyCall ???    ???           (29 ap bits)
!   3        MyCall DxCall ???           (58 ap bits)
!   4        MyCall DxCall RRR           (77 ap bits)
!   5        MyCall DxCall 73            (77 ap bits)
!   6        MyCall DxCall RR73          (77 ap bits)
!********
         naptypes(0,1:4)=(/1,2,0,0/) ! Tx6 selected (CQ)
         naptypes(1,1:4)=(/2,3,0,0/) ! Tx1
         naptypes(2,1:4)=(/2,3,0,0/) ! Tx2
         naptypes(3,1:4)=(/3,4,5,6/) ! Tx3 — aggiunto RRR(4),73(5) come FT8
         naptypes(4,1:4)=(/3,4,5,6/) ! Tx4 — aggiunto RRR(4),73(5) come FT8
         naptypes(5,1:4)=(/3,1,2,0/) ! Tx5

         mycall0=''
         hiscall0=''
         first=.false.
      endif

      l1=index(mycall,char(0))
      if(l1.ne.0) mycall(l1:)=" "
      l1=index(hiscall,char(0))
      if(l1.ne.0) hiscall(l1:)=" "
      if(mycall.ne.mycall0 .or. hiscall.ne.hiscall0) then
         apbits=0
         apbits(1)=99
         apbits(30)=99
         apmy_ru=0
         aphis_fd=0

         if(len(trim(mycall)) .lt. 3) go to 10

         nohiscall=.false.
         hiscall0=hiscall
         if(len(trim(hiscall0)).lt.3) then
            hiscall0=mycall  ! use mycall for dummy hiscall - mycall won't be hashed.
            nohiscall=.true.
         endif
         message=trim(mycall)//' '//trim(hiscall0)//' RR73'
         i3=-1
         n3=-1
         call pack77(message,i3,n3,c77)
         call unpack77(c77,1,msgsent,unpk77_success)
         if(i3.ne.1 .or. (message.ne.msgsent) .or. .not.unpk77_success) go to 10
         read(c77,'(77i1)') message77
         apmy_ru=2*mod(message77(1:28)+rvec(2:29),2)-1
         aphis_fd=2*mod(message77(30:57)+rvec(29:56),2)-1
         message77=mod(message77+rvec,2)
         call encode174_91(message77,cw)
         apbits=2*cw-1
         if(nohiscall) apbits(30)=99

10       continue
         mycall0=mycall
         hiscall0=hiscall
      endif
      ndecodes=0
      decodes=' '
      fa=nfa
      fb=nfb
      dd=iwave
      ndepth0=iand(ndepth,7)             !Extract depth bits (1,2,3)

! ndepth0=3: 3 passes, bp+osd
! ndepth0=2: 3 passes, bp only
! ndepth0=1: 1 pass, no subtraction

      max_iterations=40
      syncmin=0.80
      if(ndepth0.ge.2) syncmin=0.78
      if(ndepth0.ge.3) syncmin=0.70
      dosubtract=.true.
      doosd=.true.
      nsp=4        ! 4 subtraction passes: maggiore decode in scenari ad alta QRM
      if(ndepth0.eq.2) then
         nsp=3
         doosd=.false.
      endif
      if(ndepth0.eq.1) then
         nsp=1
         dosubtract=.false.
         doosd=.false.
      endif
      nd2=0        ! inizializza per isp=4 check (nd2 usato in elseif(isp.eq.4))

! Initialize multi-period averaging tracking for this period
      best_sync_avg=-99.0
      best_f1_avg=0.0
      best_ibest_avg=0
      got_candidate=.false.

      do isp = 1,nsp
         if(isp.eq.2) then
            if(ndecodes.eq.0) exit
            nd1=ndecodes
         elseif(isp.eq.3) then
            nd2=ndecodes-nd1
            if(nd2.eq.0) exit
         elseif(isp.eq.4) then
            nd3=ndecodes-nd1-nd2
            if(nd3.eq.0) exit
         endif

! Syncmin adattivo: soglia ridotta per pass successive (segnali più deboli
! rimangono dopo la sottrazione dei segnali forti)
         syncmin_pass=syncmin
         if(isp.ge.2) syncmin_pass=syncmin*0.88
         if(isp.ge.3) syncmin_pass=syncmin*0.76

         candidate=0.0
         ncand=0
         call timer('getcand2',0)
         call getcandidates2(dd,fa,fb,syncmin_pass,nfqso,MAXCAND,savg,candidate,   &
            ncand,sbase)
         call timer('getcand2',1)
         dobigfft=.true.
         do icand=1,ncand
            f0=candidate(1,icand)
            snr=candidate(2,icand)-1.0
            call timer('ft2_down',0)
            call ft2_downsample(dd,dobigfft,f0,cd2)  !Downsample to 32 Sam/Sym
            call timer('ft2_down',1)
            if(dobigfft) dobigfft=.false.
            sum2=sum(cd2*conjg(cd2))/(real(NMAX)/real(NDOWN))
            if(sum2.gt.0.0) cd2=cd2/sqrt(sum2)
! Sample rate is now 12000/9 = 1333.33 samples/second
            do iseg=1,3                ! DT search is done over 3 segments
               do isync=1,2
                  if(isync.eq.1) then
                     idfmin=-12
                     idfmax=12
                     idfstp=3
                     ibmin=-688
                     ibmax=2024
                     if(iseg.eq.1) then
                        ibmin=216
                        ibmax=1120
                     elseif(iseg.eq.2) then
                        smax1=smax
                        ibmin=1120
                        ibmax=2024
                     elseif(iseg.eq.3) then
                        ibmin=-688
                        ibmax=216
                     endif
                     ibstp=4
                  else
                     idfmin=idfbest-4
                     idfmax=idfbest+4
                     idfstp=1
                     ibmin=ibest-5
                     ibmax=ibest+5
                     ibstp=1
                  endif
                  ibest=-1
                  idfbest=0
                  smax=-99.
                  call timer('sync2d  ',0)
                  do idf=idfmin,idfmax,idfstp
                     do istart=ibmin,ibmax,ibstp
                        call sync2d(cd2,istart,ctwk2(:,idf),1,sync)  !Find sync power
                        if(sync.gt.smax) then
                           smax=sync
                           ibest=istart
                           idfbest=idf
                        endif
                     enddo
                  enddo
                  call timer('sync2d  ',1)
               enddo
               if(iseg.eq.1) smax1=smax
               smaxthresh=0.80
               if(ndepth0.ge.3) smaxthresh=0.65
               if(isp.ge.2) smaxthresh=smaxthresh*0.88  ! pass 2: ~15% più bassa
               if(isp.ge.3) smaxthresh=smaxthresh*0.76  ! pass 3+: ~25% più bassa
               if(smax.lt.smaxthresh) cycle
               if(iseg.gt.1 .and. smax.lt.smax1) cycle
               f1=f0+real(idfbest)
               if( f1.le.10.0 .or. f1.ge.4990.0 ) cycle
               call timer('ft2down ',0)
               call ft2_downsample(dd,dobigfft,f1,cb) !Final downsample, corrected f0
               call timer('ft2down ',1)
               sum2=sum(abs(cb)**2)/(real(NSS)*NN)
               if(sum2.gt.0.0) cb=cb/sqrt(sum2)
               cd=0.
               if(ibest.ge.0) then
                  it=min(NDMAX-1,ibest+NN*NSS-1)
                  np=it-ibest+1
                  cd(0:np-1)=cb(ibest:it)
               else
                  cd(-ibest:ibest+NN*NSS-1)=cb(0:NN*NSS+2*ibest-1)
               endif
               call timer('bitmet  ',0)
               call get_ft2_bitmetrics(cd,bitmetrics,badsync)
               call timer('bitmet  ',1)
               if(badsync) cycle

! Track best candidate near nfqso for multi-period averaging
               if(iand(ndepth,16).eq.16 .and.                    &
                  abs(f1-nfqso).lt.100.0 .and.                   &
                  smax.gt.best_sync_avg) then
                  best_bm=bitmetrics
                  best_sync_avg=smax
                  best_f1_avg=f1
                  best_ibest_avg=ibest
                  got_candidate=.true.
               endif

               hbits=0
               where(bitmetrics(:,1).ge.0) hbits=1
               ns1=count(hbits(  1:  8).eq.(/0,0,0,1,1,0,1,1/))
               ns2=count(hbits( 67: 74).eq.(/0,1,0,0,1,1,1,0/))
               ns3=count(hbits(133:140).eq.(/1,1,1,0,0,1,0,0/))
               ns4=count(hbits(199:206).eq.(/1,0,1,1,0,0,0,1/))
               nsync_qual=ns1+ns2+ns3+ns4
               nsync_qual_min=13
               if(ndepth0.ge.3) nsync_qual_min=10
               if(nsync_qual.lt.nsync_qual_min) cycle

               scalefac=2.83
               llra(  1: 58)=bitmetrics(  9: 66, 1)
               llra( 59:116)=bitmetrics( 75:132, 1)
               llra(117:174)=bitmetrics(141:198, 1)
               llra=scalefac*llra
               llrb(  1: 58)=bitmetrics(  9: 66, 2)
               llrb( 59:116)=bitmetrics( 75:132, 2)
               llrb(117:174)=bitmetrics(141:198, 2)
               llrb=scalefac*llrb
               llrc(  1: 58)=bitmetrics(  9: 66, 3)
               llrc( 59:116)=bitmetrics( 75:132, 3)
               llrc(117:174)=bitmetrics(141:198, 3)
               llrc=scalefac*llrc

! Multi-metrica: llrd = best-of (max |valore|), llre = media
               do i=1,2*ND
                  if(abs(llra(i)).ge.abs(llrb(i)) .and. &
                     abs(llra(i)).ge.abs(llrc(i))) then
                     llrd(i)=llra(i)
                  elseif(abs(llrb(i)).ge.abs(llrc(i))) then
                     llrd(i)=llrb(i)
                  else
                     llrd(i)=llrc(i)
                  endif
                  llre(i)=(llra(i)+llrb(i)+llrc(i))/3.0
               enddo

               apmag=maxval(abs(llra))*1.1
               npasses=5+nappasses(nQSOProgress)
               if(lapcqonly) npasses=6
               if(ndepth0.eq.1) npasses=5
               if(ncontest.eq.6) npasses=6  ! Fox/DXped: 5 metric + AP type 1 (CQ)
! ncontest=7 (Hound): full AP passes enabled
               do ipass=1,npasses
                  if(ipass.eq.1) llr=llra
                  if(ipass.eq.2) llr=llrb
                  if(ipass.eq.3) llr=llrc
                  if(ipass.eq.4) llr=llrd
                  if(ipass.eq.5) llr=llre
                  if(ipass.le.5) then
                     apmask=0
                     iaptype=0
                  endif

                  if(ipass .gt. 5) then
                     llrd=llrc
                     iaptype=naptypes(nQSOProgress,ipass-5)
                     if(lapcqonly) iaptype=1

! ncontest=0 : NONE
!          1 : NA_VHF
!          2 : EU_VHF
!          3 : FIELD DAY
!          4 : RTTY
!          5 : WW_DIGI
!          6 : FOX
!          7 : HOUND
!
! Conditions that cause us to bail out of AP decoding
                     napwid=75
                     if(ncontest.le.5 .and. iaptype.ge.3 .and. (abs(f1-nfqso).gt.napwid) ) cycle
                     if(iaptype.ge.2 .and. apbits(1).gt.1) cycle  ! No, or nonstandard, mycall
                     if(iaptype.ge.3 .and. apbits(30).gt.1) cycle ! No, or nonstandard, dxcall

                     if(iaptype.eq.1) then  ! CQ or CQ TEST or CQ FD or CQ RU or CQ WW
                        apmask=0
                        apmask(1:29)=1
                        if( ncontest.eq.0 ) llrd(1:29)=apmag*mcq(1:29)
                        if( ncontest.eq.1 ) llrd(1:29)=apmag*mcqtest(1:29)
                        if( ncontest.eq.2 ) llrd(1:29)=apmag*mcqtest(1:29)
                        if( ncontest.eq.3 ) llrd(1:29)=apmag*mcqfd(1:29)
                        if( ncontest.eq.4 ) llrd(1:29)=apmag*mcqru(1:29)
                        if( ncontest.eq.5 ) llrd(1:29)=apmag*mcqww(1:29)
                     endif

                     if(iaptype.eq.2) then ! MyCall,???,???
                        apmask=0
                        if(ncontest.eq.0.or.ncontest.eq.1.or.ncontest.eq.5) then
                           apmask(1:29)=1
                           llrd(1:29)=apmag*apbits(1:29)
                        else if(ncontest.eq.2) then
                           apmask(1:28)=1
                           llrd(1:28)=apmag*apbits(1:28)
                        else if(ncontest.eq.3) then
                           apmask(1:28)=1
                           llrd(1:28)=apmag*apbits(1:28)
                        else if(ncontest.eq.4) then
                           apmask(2:29)=1
                           llrd(2:29)=apmag*apmy_ru(1:28)
                        endif
                     endif

                     if(iaptype.eq.3) then ! MyCall,DxCall,???
                        apmask=0
                        if(ncontest.eq.0.or.ncontest.eq.1.or.ncontest.eq.2.or.ncontest.eq.5) then
                           apmask(1:58)=1
                           llrd(1:58)=apmag*apbits(1:58)
                        else if(ncontest.eq.3) then ! Field Day
                           apmask(1:56)=1
                           llrd(1:28)=apmag*apbits(1:28)
                           llrd(29:56)=apmag*aphis_fd(1:28)
                        else if(ncontest.eq.4) then
                           apmask(2:57)=1
                           llrd(2:29)=apmag*apmy_ru(1:28)
                           llrd(30:57)=apmag*apbits(30:57)
                        endif
                     endif

                     if(iaptype.eq.4 .or. iaptype.eq.5 .or. iaptype.eq.6) then
                        apmask=0
                        if(ncontest.le.5) then
                           apmask(1:77)=1   ! mycall, hiscall, RRR|73|RR73
                           if(iaptype.eq.6) llrd(1:77)=apmag*apbits(1:77)
                        endif
                     endif

                     llr=llrd
                  endif
                  message77=0
                  dmin=0.0

                  ndeep=3
                  maxosd=3
                  if(ndepth0.ge.3) maxosd=4
                  if(.not.doosd) maxosd = -1
                  call timer('dec174_91 ',0)
                  Keff=91
                  call decode174_91(llr,Keff,maxosd,ndeep,apmask,message91,cw, &
                                    ntype,nharderror,dmin)
                  message77=message91(1:77)
                  call timer('dec174_91 ',1)

                  if(sum(message77).eq.0) cycle
                  if( nharderror.ge.0 ) then
                     message77=mod(message77+rvec,2) ! remove rvec scrambling
                     write(c77,'(77i1)') message77(1:77)
                     call unpack77(c77,1,message,unpk77_success)
                     if(.not.unpk77_success) exit
                     if(dosubtract) then
                        call get_ft2_tones_from_77bits(message77,i4tone)
                        dt=real(ibest)/1333.33
                        call timer('subtract',0)
                        call subtractft2(dd,i4tone,f1,dt)
                        call timer('subtract',1)
                     endif
                     idupe=0
                     do i=1,ndecodes
                        if(decodes(i).eq.message) idupe=1
                     enddo
                     if(idupe.eq.1) exit
                     ndecodes=ndecodes+1
                     decodes(ndecodes)=message
                     if(snr.gt.0.0) then
                        xsnr=10*log10(snr)-13.0
                     else
                        xsnr=-21.0
                     endif
                     nsnr=nint(max(-21.0,xsnr))
                     xdt=ibest/1333.33 - 0.5
                     qual=1.0-(nharderror+dmin)/60.0
                     if(nharderror.gt.48) exit       ! too many hard errors
                     call this%callback(smax,nsnr,xdt,f1,message,iaptype,qual)
                     exit
                  endif
               enddo                      !Sequence estimation
               if(nharderror.ge.0) exit
            enddo                         !3 DT segments
         enddo                            !Candidate list
      enddo                               !Subtraction loop

! ----------------------------------------------------------------
! Multi-period averaging: accumulate and attempt averaged decode
! ----------------------------------------------------------------
      if(iand(ndepth,16).eq.16 .and. got_candidate) then
         if(navg_ft2.eq.0 .or. abs(best_f1_avg-f_avg).gt.10.0) then
! First period or frequency changed: reset accumulator
            bm_avg=best_bm
            navg_ft2=1
            f_avg=best_f1_avg
            dt_avg=real(best_ibest_avg)/1333.33
         else
! Accumulate using EMA (Exponential Moving Average)
            navg_ft2=navg_ft2+1
            ntc=min(navg_ft2,6)
            u=1.0/real(ntc)
            bm_avg=u*best_bm + (1.0-u)*bm_avg
            f_avg=u*best_f1_avg + (1.0-u)*f_avg
            dt_avg=u*real(best_ibest_avg)/1333.33 + (1.0-u)*dt_avg
         endif

! Write averaging status to avemsg.txt (unit 14)
         write(14,1200) navg_ft2,nint(f_avg),dt_avg
1200     format('FT2 avg:  navg=',i3,'  f=',i5,' Hz  dt=',f6.2,' s')

! Try averaged decode if single-period failed and navg >= 2
         if(ndecodes.eq.0 .and. navg_ft2.ge.2) then
            scalefac=2.83
            llra(  1: 58)=bm_avg(  9: 66, 1)
            llra( 59:116)=bm_avg( 75:132, 1)
            llra(117:174)=bm_avg(141:198, 1)
            llra=scalefac*llra
            llrb(  1: 58)=bm_avg(  9: 66, 2)
            llrb( 59:116)=bm_avg( 75:132, 2)
            llrb(117:174)=bm_avg(141:198, 2)
            llrb=scalefac*llrb
            llrc(  1: 58)=bm_avg(  9: 66, 3)
            llrc( 59:116)=bm_avg( 75:132, 3)
            llrc(117:174)=bm_avg(141:198, 3)
            llrc=scalefac*llrc

! Multi-metrica: llrd = best-of (max |valore|), llre = media
            do i=1,2*ND
               if(abs(llra(i)).ge.abs(llrb(i)) .and.             &
                  abs(llra(i)).ge.abs(llrc(i))) then
                  llrd(i)=llra(i)
               elseif(abs(llrb(i)).ge.abs(llrc(i))) then
                  llrd(i)=llrb(i)
               else
                  llrd(i)=llrc(i)
               endif
               llre(i)=(llra(i)+llrb(i)+llrc(i))/3.0
            enddo

            apmag=maxval(abs(llra))*1.1
            npasses=5+nappasses(nQSOProgress)
            if(lapcqonly) npasses=6
            if(ndepth0.eq.1) npasses=5
            if(ncontest.eq.6) npasses=5

            do ipass=1,npasses
               if(ipass.eq.1) llr=llra
               if(ipass.eq.2) llr=llrb
               if(ipass.eq.3) llr=llrc
               if(ipass.eq.4) llr=llrd
               if(ipass.eq.5) llr=llre
               if(ipass.le.5) then
                  apmask=0
                  iaptype=0
               endif

               if(ipass .gt. 5) then
                  llrd=llrc
                  iaptype=naptypes(nQSOProgress,ipass-5)
                  if(lapcqonly) iaptype=1
                  napwid=75
                  if(ncontest.le.5 .and. iaptype.ge.3 .and.      &
                     (abs(f_avg-nfqso).gt.napwid) ) cycle
                  if(iaptype.ge.2 .and. apbits(1).gt.1) cycle
                  if(iaptype.ge.3 .and. apbits(30).gt.1) cycle

                  if(iaptype.eq.1) then
                     apmask=0
                     apmask(1:29)=1
                     if( ncontest.eq.0 ) llrd(1:29)=apmag*mcq(1:29)
                     if( ncontest.eq.1 ) llrd(1:29)=apmag*mcqtest(1:29)
                     if( ncontest.eq.2 ) llrd(1:29)=apmag*mcqtest(1:29)
                     if( ncontest.eq.3 ) llrd(1:29)=apmag*mcqfd(1:29)
                     if( ncontest.eq.4 ) llrd(1:29)=apmag*mcqru(1:29)
                     if( ncontest.eq.5 ) llrd(1:29)=apmag*mcqww(1:29)
                  endif

                  if(iaptype.eq.2) then
                     apmask=0
                     if(ncontest.eq.0.or.ncontest.eq.1.or.ncontest.eq.5) then
                        apmask(1:29)=1
                        llrd(1:29)=apmag*apbits(1:29)
                     else if(ncontest.eq.2) then
                        apmask(1:28)=1
                        llrd(1:28)=apmag*apbits(1:28)
                     else if(ncontest.eq.3) then
                        apmask(1:28)=1
                        llrd(1:28)=apmag*apbits(1:28)
                     else if(ncontest.eq.4) then
                        apmask(2:29)=1
                        llrd(2:29)=apmag*apmy_ru(1:28)
                     endif
                  endif

                  if(iaptype.eq.3) then
                     apmask=0
                     if(ncontest.eq.0.or.ncontest.eq.1.or.         &
                        ncontest.eq.2.or.ncontest.eq.5) then
                        apmask(1:58)=1
                        llrd(1:58)=apmag*apbits(1:58)
                     else if(ncontest.eq.3) then
                        apmask(1:56)=1
                        llrd(1:28)=apmag*apbits(1:28)
                        llrd(29:56)=apmag*aphis_fd(1:28)
                     else if(ncontest.eq.4) then
                        apmask(2:57)=1
                        llrd(2:29)=apmag*apmy_ru(1:28)
                        llrd(30:57)=apmag*apbits(30:57)
                     endif
                  endif

                  if(iaptype.eq.4 .or. iaptype.eq.5 .or.         &
                     iaptype.eq.6) then
                     apmask=0
                     if(ncontest.le.5) then
                        apmask(1:77)=1
                        if(iaptype.eq.6) llrd(1:77)=apmag*apbits(1:77)
                     endif
                  endif

                  llr=llrd
               endif
               message77=0
               dmin=0.0

               ndeep=3
               maxosd=3
               if(abs(nfqso-f_avg).le.75.0 .and. ndepth0.ge.3) then
                  maxosd=4
               endif
               if(.not.doosd) maxosd = -1
               Keff=91
               call decode174_91(llr,Keff,maxosd,ndeep,apmask,   &
                                 message91,cw,ntype,nharderror,dmin)
               message77=message91(1:77)

               if(sum(message77).eq.0) cycle
               if( nharderror.ge.0 ) then
                  message77=mod(message77+rvec,2)
                  write(c77,'(77i1)') message77(1:77)
                  call unpack77(c77,1,message,unpk77_success)
                  if(.not.unpk77_success) exit
                  ndecodes=ndecodes+1
                  decodes(ndecodes)=message
                  xsnr=-21.0
                  xdt=dt_avg - 0.5
                  qual=1.0-(nharderror+dmin)/60.0
! Report averaged decode with "a" flag via callback
                  call this%callback(best_sync_avg,nint(xsnr),    &
                       xdt,f_avg,message,iaptype,qual)
! Write decoded message to avemsg.txt
                  write(14,1210) navg_ft2,nint(f_avg),xdt,        &
                       trim(message)
1210              format('FT2 avg:  navg=',i3,'  f=',i5,          &
                       '  dt=',f6.2,'  ',a)
                  navg_ft2=0              !Reset after successful decode
                  bm_avg=0.0
                  exit
               endif
            enddo                         !ipass (averaged decode)
         endif
      endif

      return
   end subroutine decode

! Clear multi-period averaging buffers (called on mode/band change)
   subroutine ft2_clravg()
      navg_ft2=0
      bm_avg=0.0
      f_avg=0.0
      dt_avg=0.0
   end subroutine ft2_clravg

end module ft2_decode
