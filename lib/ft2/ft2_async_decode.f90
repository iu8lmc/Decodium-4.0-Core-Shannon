module ft2_async_mod
! Module for async (sliding-window) FT2 decoding.
! Provides a standalone subroutine callable from C++ via QtConcurrent.

  use ft2_decode
  implicit none

  integer, parameter :: MAX_ASYNC_LINES = 100
  integer, save :: nasync_lines = 0
  character(len=80), save :: async_results(MAX_ASYNC_LINES)

  type, extends(ft2_decoder) :: async_ft2_dec
  end type

contains

  subroutine async_ft2_callback(this, sync, snr, dt, freq, decoded, nap, qual)
    class(ft2_decoder), intent(inout) :: this
    real, intent(in) :: sync
    integer, intent(in) :: snr
    real, intent(in) :: dt
    real, intent(in) :: freq
    character(len=37), intent(in) :: decoded
    integer, intent(in) :: nap
    real, intent(in) :: qual
    character(len=2) :: annot
    character(len=37) :: decoded0

    decoded0 = decoded
    annot = '  '
    if(nap.ne.0) then
       write(annot,'(a1,i1)') 'a', nap
       if(qual.lt.0.17) decoded0(37:37)='?'
    endif

    if(nasync_lines < MAX_ASYNC_LINES) then
       nasync_lines = nasync_lines + 1
       write(async_results(nasync_lines), 1001) snr, dt, nint(freq), decoded0, annot
1001   format(i4,f5.1,i5,' ~ ',1x,a37,1x,a2)
    endif
  end subroutine async_ft2_callback

end module ft2_async_mod


subroutine ft2_async_decode(iwave, nqsoprogress, nfqso, nfa, nfb, &
     ndepth, ncontest, mycall, hiscall, outlines, nout)

! Standalone FT2 decoder for async (sliding-window) mode.
! Called from C++ via QtConcurrent::run, like fast_decode_ for MSK144.

  use ft2_async_mod

  include 'ft2_params.f90'

  integer*2 iwave(NMAX)
  integer nqsoprogress, nfqso, nfa, nfb, ndepth, ncontest
  character*12 mycall, hiscall
  character*80 outlines(100)
  integer nout

  type(async_ft2_dec) :: dec
  integer i

! Clear results
  nasync_lines = 0
  outlines = ' '

! Call FT2 decoder with async callback
  call dec%decode(async_ft2_callback, iwave, nqsoprogress, nfqso, &
       nfa, nfb, ndepth, .false., ncontest, mycall, hiscall)

! Copy results to output
  nout = nasync_lines
  do i = 1, nout
     outlines(i) = async_results(i)
  enddo

  return
end subroutine ft2_async_decode
