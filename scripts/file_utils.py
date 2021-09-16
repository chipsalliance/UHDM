import shutil
import os


def set_content_if_changed(filename, content):
    if os.path.exists(filename):
        with open(filename, 'rb') as strm:
            orig_content = strm.read()

        if orig_content == content:
            return False

    with open(filename, 'wt') as strm:
        strm.write(content)

    return True


def copy_file_if_changed(src, dst):
    '''
    Copy file from source to destination if destination does not exist or
    its content is different.
    '''
    src_content = None
    if os.path.exists(src):
        with open(src, 'rb') as strm:
            src_content = strm.read()

    dst_content = None
    if os.path.exists(dst):
        with open(dst, 'rb') as strm:
            dst_content = strm.read()

    if src_content and (src_content == dst_content):
        return False

    shutil.copyfile(src, dst)
    return True


def find_file(basedir, filename):
    for subdir, dirs, filenames in os.walk(basedir):
        if filename in filenames:
            return os.path.join(subdir, filename)

    return None

def remove_file_safely(filepath):
    if os.path.isfile(filepath):
        os.remove(filepath)
        return True

    return False
