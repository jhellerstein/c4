require 'rubygems'
require 'ffi'

class C4
  module Col
    extend FFI::Library
    ffi_lib 'c4'
    attach_function 'c4_initialize', [], :void
    attach_function 'c4_make', [:int], :pointer
    attach_function 'c4_install_file', [:pointer, :string], :int
    attach_function 'c4_install_str', [:pointer, :string], :int
    attach_function 'c4_dump_table', [:pointer, :string], :string
    attach_function 'c4_destroy', [:pointer], :void
    attach_function 'c4_terminate', [], :void
  end

  def initialize(port=0)
    Col.c4_initialize
    @c4 = Col.c4_make(port)
  end

  # TODO: check status
  def install_prog(inprog)
    s = Col.c4_install_file(@c4, inprog)
  end

  # TODO: check status
  def install_str(inprog)
    s = Col.c4_install_str(@c4, inprog)
  end
  
  def dump_table(tbl_name)
    Col.c4_dump_table(@c4, tbl_name)
  end

  def finish
    Col.c4_destroy(@c4)
    Col.c4_terminate
  end
end