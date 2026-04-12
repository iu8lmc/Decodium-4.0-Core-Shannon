subroutine my_hash(mycall)

  character*(*) mycall
  character*13 c13

  c13=mycall//'          '
  call ftx_pack77_reset_context()
  call ftx_pack77_save_hash_call(c13,n10,n12,n22)
  
  return
end subroutine my_hash
