class FileMutex:
    def __init__(self, path: str): ...
    def lock(self): ...
    def unlock(self): ...
    def try_lock(self) -> bool: ...
    def lock_shared(self): ...
    def unlock_shared(self): ...
    def try_lock_shared(self) -> bool: ...
    def file_path(self) -> str: ...
