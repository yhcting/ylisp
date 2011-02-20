import java.net.*;
import java.io.*;

interface SockListener {
    boolean onSocketRead(String msg);
}

class Sock {
    private Socket            _sock;
    private DataOutputStream  _out;
    private DataInputStream   _in;
    private SockListener      _listener;
    private Thread            _readt = new Thread () {
        public void run() {
            int     sz;
            byte[]  d;
            int     br;
            while (true) {
                try {
                    sz = _in.readInt();
                    d = new byte[sz];
                    br = _in.read(d);
                    if (br != sz) {
                        System.out.println("Invalid pdu packet! Ignore it!");
                    } else {
                        _listener.onSocketRead(new String(d, "UTF-8"));
                    }
                } catch (EOFException e) {
                    System.out.print(e.getMessage());
                    System.exit(0);
                } catch (UTFDataFormatException e) {
                    System.out.print(e.getMessage());
                } catch (IOException e) {
                    System.out.print(e.getMessage());
                    System.exit(0);
                }
            }
        }
    };
   
    
    private static final byte[] _intToByteArrayBE(int v) {
        return new byte[] {
                (byte)(v >>> 24), (byte)(v >>> 16),
                (byte)(v >>> 8),  (byte)v};
    }    
    
    boolean send (String msg) {
        try{
            byte[] b = msg.getBytes("UTF-8");
            _out.write(_intToByteArrayBE(b.length));
            _out.write(b);
            _out.flush();
        } catch(IOException ioException){
            ioException.printStackTrace();
            return false;
        }
        return true;
    }
    
    
    boolean init (SockListener listener, String server, int port) {
        boolean  r = false;
        try {
            _sock = new Socket (server, port);
            _out = new DataOutputStream (_sock.getOutputStream());
            _in  = new DataInputStream (_sock.getInputStream());
            _listener = listener;
            _readt.start();
            r = true;
        } catch (UnknownHostException e) {
            System.err.println ("Unknown host");
        } catch (IOException e) {
            e.printStackTrace();
        }

        return r;
    }
    
    
}
