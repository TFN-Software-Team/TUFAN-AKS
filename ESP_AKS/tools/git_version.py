import subprocess
import os

Import('env')

def get_git_info():
    try:
        git_hash = subprocess.check_output(
            ['git', 'rev-parse', '--short', 'HEAD'],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        git_hash = 'unknown'
    
    try:
        git_version = subprocess.check_output(
            ['git', 'describe', '--tags', '--always', '--dirty'],
            stderr=subprocess.DEVNULL
        ).decode().strip()
    except Exception:
        git_version = git_hash
    
    return git_hash, git_version

git_hash, git_version = get_git_info()

env.Append(CPPDEFINES=[
    ('GIT_HASH', env.StringifyMacro(git_hash)),
    ('FW_VERSION', env.StringifyMacro(git_version)),
    ('BUILD_DATE', env.StringifyMacro(__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M')))
])
