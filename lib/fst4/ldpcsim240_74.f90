program ldpcsim240_74

! End-to-end test of the (240,74)/crc24 encoder and decoders.

   use ftx_pack77_c_api, only: ftx_pack77_reset_context, ftx_pack77_pack

   parameter(N=240, NN=120)
   character*8 arg
   character*37 msg0,msgsent
   character*77 c77
   character*24 c24
   integer*1 msgbits(101)
   integer*1 apmask(240)
   integer*1 cw(240)
   integer*1 codeword(N),message74(74)
   integer ncrc24
   integer modtype, graymap(0:3)
   integer*4 itone(120)
   integer channeltype
   integer lmax(1)
   real rxdata(N)
   real llr(240)
   real bitmetrics(2*NN,4)
   complex c1(4,8),c2(16,4),c4(256,2),cs(0:3,NN)
   real s2(0:65535)
   logical one(0:65535,0:15)    ! 65536 8-symbol sequences, 16 bits
   logical first,unpk77_success
   data first/.true./
   data graymap/0,1,3,2/

   nargs=iargc()
   if(nargs.ne.7 .and. nargs.ne.8) then
      print*,'Usage: ldpcsim       maxosd norder #trials    s  Keff  modtype  channel '
      print*,'e.g.   ldpcsim240_74   2      4     1000    0.85  50      1        0'
      print*,'s    : if negative, then value is ignored and sigma is calculated from SNR.'
      print*,'maxosd<0: do bp only'
      print*,'maxosd=0: do bp and then call osd once with channel llrs.'
      print*,'maxosd>0: do bp and then call osc maxosd times with saved bp outputs.'
      print*,'norder  : osd decoding depth'
      print*,'Keff    : # of message bits, Keff must be in the range 50:74'
      print*,'modtype : 0 coherent BPSK, 1 4FSK'
      print*,'channel : 0 AWGN, 1 Rayleigh (4FSK only)'
      print*,'WSPR-format message is optional'
      return
   endif
   call getarg(1,arg)
   read(arg,*) maxosd
   call getarg(2,arg)
   read(arg,*) norder
   call getarg(3,arg)
   read(arg,*) ntrials
   call getarg(4,arg)
   read(arg,*) s
   call getarg(5,arg)
   read(arg,*) Keff
   call getarg(6,arg)
   read(arg,*) modtype
   call getarg(7,arg)
   read(arg,*) channeltype
   call getarg(8,arg)

   msg0='K9AN EN50 20                       '
   call ftx_pack77_reset_context()
   call ftx_pack77_pack(msg0,i3,n3,c77,msgsent,unpk77_success,0)

   rate=real(Keff)/real(N)

   write(*,*) "code rate: ",rate
   write(*,*) "maxosd   : ",maxosd
   write(*,*) "norder   : ",norder
   write(*,*) "s        : ",s
   write(*,*) "K        : ",Keff
   if(modtype.eq.0) write(*,*) "modtype  : coherent BPSK"
   if(modtype.eq.1) write(*,*) "modtype  : noncoherent 4FSK"
   if(channeltype.eq.0) write(*,*) "channel  : AWGN"
   if(channeltype.eq.1) write(*,*) "channel  : Rayleigh"

   msgbits=0
   read(c77,'(50i1)') msgbits(1:50)
   write(*,*) 'message'
   write(*,'(50i1)') msgbits(1:50)

   call get_crc24(msgbits,74,ncrc24)
   write(c24,'(b24.24)') ncrc24
   read(c24,'(24i1)') msgbits(51:74)
   write(*,'(24i1)') msgbits(51:74)
   write(*,*) 'message with crc24'
   write(*,'(74i1)') msgbits(1:74)

   call encode240_74(msgbits(1:74),codeword)
   do i=1,120
     is=codeword(2*i)+2*codeword(2*i-1)
     itone(i)=graymap(is) 
   enddo

   write(*,*) 'codeword'
   write(*,'(77i1,1x,24i1,1x,73i1)') codeword

!   call init_random_seed()
!   call sgran()

   iq=1
   one=.false.
   do i=0,65535
      do j=0,15
         if(iand(i,2**j).ne.0) one(i,j)=.true.
      enddo
   enddo

   write(*,*) "Eb/N0    Es/N0   ngood  nundetected   symbol error rate"
   do idb = 24,-8,-1
      db=idb/2.0-1.0
      sigma=1/sqrt( 2*rate*iq*(10**(db/10.0)) )  ! to make db represent Eb/No
!  sigma=1/sqrt( 2*(10**(db/10.0)) )        ! db represents Es/No
      ngood=0
      nue=0
      nberr=0
      nsymerr=0

      do itrial=1, ntrials
! Create a realization of a noisy received word
         if(modtype.eq.0) then
            iq = 1 ! bits per symbol
            sigma=1/sqrt( 2*rate*iq*(10**(db/10.0)) )  ! to make db represent Eb/No
            do i=1,N
               rxdata(i) = 2.0*codeword(i)-1.0 + sigma*gran()
            enddo
            nerr=0
            do i=1,N
               if( rxdata(i)*(2*codeword(i)-1.0) .lt. 0 ) nerr=nerr+1
            enddo
            nberr=nberr+nerr

            rxav=sum(rxdata)/N
            rx2av=sum(rxdata*rxdata)/N
            rxsig=sqrt(rx2av-rxav*rxav)
            rxdata=rxdata/rxsig
            if( s .lt. 0 ) then
               ss=sigma
            else
               ss=s
            endif

            llr=2.0*rxdata/(ss*ss)
         else
! noncoherent MFSK
            iq = 2 ! bits per symbol
            sigma=1/sqrt( 2*rate*iq*(10**(db/10.0)) )  ! to make db represent Eb/No
            A=1
            do i=1,120
               do j=0,3
                  if(j.eq.itone(i)) then
                     if(channeltype.eq.0) then
                        A=1.0
                     elseif(channeltype.eq.1) then
                        xI=gran()**2+gran()**2
                        A=sqrt(xI/2)
                     endif
                     cs(j,i)= A + sigma*gran() + cmplx(0,1)*sigma*gran()
                  elseif(j.ne.itone(i)) then
                     cs(j,i)=     sigma*gran() + cmplx(0,1)*sigma*gran()
                  endif
               enddo
               lmax=maxloc(abs(cs(:,i)))
               if(lmax(1)-1.ne.itone(i) ) nsymerr=nsymerr+1
            enddo

            do k=1,NN,8

               do m=1,8  ! do 4 1-symbol correlations for each of 8 symbs
                  s2=0
                  do is=1,4
                     c1(is,m)=cs(graymap(is-1),k+m-1)
                     s2(is-1)=abs(c1(is,m))
                  enddo
                  ipt=(k-1)*2+2*(m-1)+1
                  do ib=0,1
                     bm=maxval(s2(0:3),one(0:3,1-ib)) - &
                        maxval(s2(0:3),.not.one(0:3,1-ib))
                     if(ipt+ib.gt.2*NN) cycle
                     bitmetrics(ipt+ib,1)=bm
                  enddo
               enddo

               do m=1,4  ! do 16 2-symbol correlations for each of 4 2-symbol groups
                  s2=0
                  do i=1,4
                     do j=1,4
                        is=(i-1)*4+j
                        c2(is,m)=c1(i,2*m-1)+c1(j,2*m)
                        s2(is-1)=abs(c2(is,m))**2
                     enddo
                  enddo
                  ipt=(k-1)*2+4*(m-1)+1
                  do ib=0,3
                     bm=maxval(s2(0:15),one(0:15,3-ib)) - &
                        maxval(s2(0:15),.not.one(0:15,3-ib))
                     if(ipt+ib.gt.2*NN) cycle
                     bitmetrics(ipt+ib,2)=bm
                  enddo
               enddo

               do m=1,2 ! do 256 4-symbol corrs for each of 2 4-symbol groups
                  s2=0
                  do i=1,16
                     do j=1,16
                        is=(i-1)*16+j
                        c4(is,m)=c2(i,2*m-1)+c2(j,2*m)
                        s2(is-1)=abs(c4(is,m))
                     enddo
                  enddo
                  ipt=(k-1)*2+8*(m-1)+1
                  do ib=0,7
                     bm=maxval(s2(0:255),one(0:255,7-ib)) - &
                        maxval(s2(0:255),.not.one(0:255,7-ib))
                     if(ipt+ib.gt.2*NN) cycle
                     bitmetrics(ipt+ib,3)=bm
                  enddo
               enddo

               s2=0 ! do 65536 8-symbol correlations for the entire group
               do i=1,256
                  do j=1,256
                     is=(i-1)*256+j
                     s2(is-1)=abs(c4(i,1)+c4(j,2))
                  enddo
               enddo
               ipt=(k-1)*2+1
               do ib=0,15
                  bm=maxval(s2(0:65535),one(0:65535,15-ib)) - &
                     maxval(s2(0:65535),.not.one(0:65535,15-ib))
                  if(ipt+ib.gt.2*NN) cycle
                  bitmetrics(ipt+ib,4)=bm
               enddo

            enddo

            call normalizebmet(bitmetrics(:,1),2*NN)
            call normalizebmet(bitmetrics(:,2),2*NN)
            call normalizebmet(bitmetrics(:,3),2*NN)
            call normalizebmet(bitmetrics(:,4),2*NN)

            scalefac=2.83
            bitmetrics=scalefac*bitmetrics

            llr=bitmetrics(:,1)
         endif

         apmask=0
         dmin=0.0
         call decode240_74(llr, Keff, maxosd, norder, apmask, message74, cw, ntype, nharderror, dmin)
         if(nharderror.ge.0) then
            n2err=0
            do i=1,N
               if( cw(i).ne.codeword(i) ) n2err=n2err+1
            enddo
            if(n2err.eq.0) then
               ngood=ngood+1
            else
               nue=nue+1
            endif
         endif
      enddo
!      snr2500=db+10*log10(200.0/116.0/2500.0)
      esn0=db+10*log10(rate*iq)
      pberr=real(nberr)/real(ntrials*N)
      pserr=real(nsymerr)/real(ntrials*120)
      write(*,"(f4.1,4x,f5.1,1x,i8,1x,i8,8x,e10.3)") db,esn0,ngood,nue,pserr

   enddo

end program ldpcsim240_74
