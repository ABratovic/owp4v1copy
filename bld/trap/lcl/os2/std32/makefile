#pmake: lcl os_os2 cpu_386 trap
#
# Create the OS/2 STD32.DLL (16-bit version)
#
host_CPU = i86
host_OS = os2

os      = os2
srv     = lcl
cpu     = x86
name    = std32
which = TRAP

extra_c_flags =-i"$(lang_root)/h/os21x" -i"$(lang_root)/h/os2"

extra_objs = &
        accmisc.obj &
        os2v2acc.obj &
        os2rfx.obj &
        pgmexec.obj &
        softmode.obj &
        dbgthrd.obj &
        os2conv.obj &
        os2call.obj &
        loadstd.obj &
        pmlock.obj &
        x86cpu.obj &
        supp.obj &
        core.obj

extra_linker = &
        imp DOS16SELTOFLAT DOSCALLS.424 &
        exp TellHandles_.4,TellHardMode_.5 &
        lib os2

!include $(trap_dir)/lcl/lcl.mif
!include $(trap_dir)/master.mif
