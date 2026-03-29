subroutine parse77(msg,i3,n3)

  use ftx_pack77_c_api, only: ftx_pack77_reset_context, ftx_pack77_pack
  character msg*37,c77*77
  character msgsent*37
  logical ok

  i3=-1
  n3=-1
  call ftx_pack77_reset_context()
  call ftx_pack77_pack(msg,i3,n3,c77,msgsent,ok,0)

  return
end subroutine parse77
