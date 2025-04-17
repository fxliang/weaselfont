target('weaselfont')
  set_kind('binary')
  set_languages('c++17')
  add_files('./src/*.cpp', './src/weaselfont.rc')
  add_includedirs('./src', './include')
  add_defines('UNICODE', '_UNICODE', 'WINVER=0x0603', {force=true})
  add_links('d2d1', 'dwrite', 'user32', "Shlwapi", "Shcore", "gdi32", "Comctl32")
  add_cxflags('/utf-8')

  after_build(function(target)
    local prjoutput = path.join(target:targetdir(), target:filename())
    print("copy " .. prjoutput .. " to $(projectdir)")
    os.trycp(prjoutput, "$(projectdir)")
  end)

  on_load(function (target)
    try {
      function()
        os.run('taskkill.exe /im '.. target:filename() .. ' /F')
        print('killed ' .. target:filename() .. ' before build done!')
      end
    } catch {}
  end)
