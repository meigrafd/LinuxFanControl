import json, socket, os
SOCK = os.environ.get("LFCD_SOCK", "/tmp/lfcd.sock")

class LfcdClient:
    def __init__(self, path=SOCK):
        self.path = path
        self._id = 0
    def _call(self, method, **params):
        self._id += 1
        req = {"id": self._id, "method": method, "params": params}
        s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        s.connect(self.path)
        s.sendall((json.dumps(req)+"\n").encode())
        buf=b""
        while not buf.endswith(b"\n"):
            part = s.recv(65536)
            if not part: break
            buf+=part
        s.close()
        rsp = json.loads(buf.decode().rstrip("\n"))
        if "error" in rsp:
            raise RuntimeError(rsp["error"])
        return rsp.get("result")
    # public
    def get_version(self): return self._call("getVersion")
    def enumerate(self):   return self._call("enumerate")
    def read_temp(self, path): return self._call("readTemp", path=path)
    def probe_pwm(self, label): return self._call("probePwm", label=label)
    def write_pwm(self, pwm_path, pct): return self._call("writePwm", pwm_path=pwm_path, pct=float(pct))
    def set_enable(self, enable_path, mode=1): return self._call("setEnable", enable_path=enable_path, mode=int(mode))
