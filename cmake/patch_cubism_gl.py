"""
Patch CubismNativeFramework's InitializeGlFunctions() to copy GL function
pointers from GLEW's already-initialized __glew* globals instead of using
wglGetProcAddress() which conflicts with QOpenGLContext.

Usage: python patch_cubism_gl.py <file_path>
"""
import re
import sys

def main():
    filepath = sys.argv[1]

    with open(filepath, 'r', encoding='utf-8') as f:
        content = f.read()

    if 'OAI_GLEW_COPY_FROM_GLEW' in content:
        print('Already patched, skipping.')
        return

    # Replace each: varname = (CAST)WinGlGetProcAddress("name");
    # With:         varname = ::__glewName;
    # Use :: prefix to reference GLEW's GLOBAL variable, not the local one
    # that shadows it in the anonymous namespace.
    # GLEW naming: glActiveTexture -> __glewActiveTexture (strip "gl" prefix)
    def repl(m):
        varname = m.group(1)
        glname = m.group(2)
        # Strip "gl" prefix to get the GLEW variable name
        # e.g. glActiveTexture -> ::__glewActiveTexture
        if glname.startswith('gl'):
            glew_name = '::__glew' + glname[2:]
        else:
            glew_name = '::__glew' + glname[0].upper() + glname[1:]
        return f'{varname} = {glew_name}; /* OAI_GLEW_COPY */'

    pattern = r'(\w+) = \(\w+\)WinGlGetProcAddress\("(\w+)"\)'
    count = len(re.findall(pattern, content))
    content = re.sub(pattern, repl, content)

    # Mark as patched
    content = content.replace(
        's_isInitializeGlFunctionsSuccess = true; ',
        's_isInitializeGlFunctionsSuccess = true; /* OAI_GLEW_COPY_FROM_GLEW */ ',
        1
    )

    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(content)

    print(f'Patched {count} GL function assignments to use GLEW globals.')

if __name__ == '__main__':
    main()
