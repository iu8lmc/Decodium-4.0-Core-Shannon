subroutine pack77_tx_c(msg0, i3, n3, bits77, msgsent, success) bind(C, name="pack77_tx_c")
  use, intrinsic :: iso_c_binding
  use ftx_pack77_c_api, only: ftx_pack77_reset_context,           &
       ftx_pack77_pack
  implicit none

  character(kind=c_char), intent(in) :: msg0(37)
  integer(c_int), intent(out) :: i3
  integer(c_int), intent(out) :: n3
  integer(c_signed_char), intent(out) :: bits77(77)
  character(kind=c_char), intent(out) :: msgsent(38)
  logical(c_bool), intent(out) :: success

  character(len=37) :: fmsg
  character(len=37) :: fmsgsent
  character(len=77) :: c77
  integer :: ibits(77)
  integer :: i
  logical :: unpk77_success

  fmsg = ' '
  fmsgsent = ' '
  c77 = '0'
  ibits = 0
  i3 = -1
  n3 = -1
  success = .false._c_bool
  msgsent = c_null_char

  do i = 1, 37
     if (msg0(i) == c_null_char) exit
     fmsg(i:i) = transfer(msg0(i), ' ')
  end do

  call ftx_pack77_reset_context()
  call ftx_pack77_pack(fmsg, i3, n3, c77, fmsgsent, unpk77_success, 0)

  if (unpk77_success) then
     read(c77, '(77i1)', err=100) ibits
     do i = 1, 77
        bits77(i) = int(ibits(i), c_signed_char)
     end do
     success = .true._c_bool
  else
     bits77 = 0_c_signed_char
  end if

100 continue
  do i = 1, 37
     msgsent(i) = transfer(fmsgsent(i:i), c_null_char)
  end do
  msgsent(38) = c_null_char
end subroutine pack77_tx_c
