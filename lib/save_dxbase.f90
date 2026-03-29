subroutine save_dxbase(dxbase0)

  use ftx_pack77_c_api, only: ftx_pack77_set_dxbase
  character*6 dxbase0

  call ftx_pack77_set_dxbase(dxbase0)

  return
end subroutine save_dxbase
