import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.util.*;

public class Main extends JFrame {
    private static final int       _FRAME_WIDTH        = 640;
    private static final int       _FRAME_HEIGHT       = 256;

    // AC : Auto Complete - See native code.
    private static final int       _AC_HANDLED         = 0; // already handled at the natives.
    private static final int       _AC_MORE_PREFIX     = 1; // there is more possible-common-prefix
    private static final int       _AC_COMPLETE        = 2; // only one candidate exists and is found.
    
    private static final int       _HISTORY_SIZE      = 100;
    
    // These log level value should match native ylisp's implementation.
    private static enum _LogLv {
        Verbose     (0),
        Develop     (1),
        Infomation  (2),
        Warn        (3),
        Error       (4);
        
        private int     _v;
        
        _LogLv(int v) { _v = v; }
        int    v()   { return _v; }
        
        static String getName(int v) {
            for(_LogLv log : _LogLv.values()) {
                if(log.v() == v) { return log.name(); }
            }
            return "Unknown";
        }
        
    }

    private YLJEditArea         _edit;
    private LinkedList<String>  _history = new LinkedList<String>();
    private int                 _hi = -1;  // history index
    private int                 _loglv = _LogLv.Warn.v(); // default is log ouput - refer native code's implementation
    private boolean             _binterp = false;
    private Object              _interpobj = new Object();
    private Thread              _interpthd = new Thread( new Runnable() {
        public void run() {
            while(true) {
                // We should 'synchroized' before notify/wait !!!
                synchronized (_interpobj) {
                    try {
                        _interpobj.wait();
                    } catch (Exception e){
                        System.out.println("Fail to wait object!!");
                    }
                    System.out.print("====================== Interpret =====================\n" +
                            _edit.getText() + "\n" +
                            "----------------------------\n\n");
                    nativeInterpret(_edit.getText());
                    addToHistory(_edit.getText());
                    _hi = -1;
                    _edit.setText(""); // clean
                    _binterp = false;
                }
            }
        }
    });
    
    // ============================= ACTIONS START ==============================
    private class InterpretAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(!_binterp) {
                synchronized(_interpobj) {
                        _binterp  = true;
                        _interpobj.notify();
                }
            } else {
                System.out.println("--- Previous interpreting request is under execution!\n");
            }
        }
    }

    private class InterruptAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            System.out.println("----interrupt----\n");
            nativeForceStop();
        }
    }
    
    private class InceaseLogLevelAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_loglv > _LogLv.Verbose.v()) { 
                _loglv--;
                nativeSetLogLevel(_loglv);
                System.out.print(">>> Current Log Level : " + _LogLv.getName(_loglv) + "\n");
            }
        }
    }
    
    private class DecreaseLogLevelAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_loglv < _LogLv.Error.v()) { 
                _loglv++;
                nativeSetLogLevel(_loglv);
                System.out.print(">>> Current Log Level : " + _LogLv.getName(_loglv) + "\n");
            }
        }
    }
    
    private class NextCommandAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_hi > 0) {
                _hi--;
                _edit.setText((String)_history.get(_hi));
            } else {
                _hi = -1;
                _edit.setText("");
            }
        }
    }

    private class PrevCommandAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_hi < _history.size()-1) {
                _hi++;                
                _edit.setText((String)_history.get(_hi));
            } else {
                _hi = _history.size();
                _edit.setText("");
            }
        }
    }

    private class AutoCompleteAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            int    maxi, i, j;
            char[] delimiters = {' ', '\n', '\'', '\"', '(', ')', '\t'};
            String orig = _edit.getText();
            int    caretpos = _edit.getCaretPosition();
            String pre   = orig.substring(0, caretpos);
            String post  = orig.substring(caretpos);
            
            maxi = -1;
            for(i=0; i<delimiters.length; i++) {
                j = pre.lastIndexOf(delimiters[i]);
                if(maxi<(j+1)) { maxi = j+1; }
            }
            
            { // Just Scope
                int r;
                if(0 <= maxi) {
                    // Normal case
                    r = nativeAutoComplete(pre.substring(maxi)); 
                } else {
                    // In case of first word
                    r = nativeAutoComplete(pre.substring(0)); 
                }
                switch(r) {
                    case _AC_MORE_PREFIX: {
                        String more = nativeGetLastNativeMessage();
                        _edit.setText(pre + more + post);
                        _edit.setCaretPosition(caretpos + more.length());
                    } break;
                    
                    case _AC_COMPLETE: {
                        String more = nativeGetLastNativeMessage();
                        more = more + " ";
                        _edit.setText(pre + more + post);
                        _edit.setCaretPosition(caretpos + more.length());
                    } break;
                    
                    default: // error case
                        ; // nothing to do
                }
            } // Just Scope
        }
    }
    
    
    
    // ============================= ACTIONS END ==============================
    
    public Main() {
        // Setup UI Frame
        setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
     
        _edit = new YLJEditArea();
        _edit.setFont(new Font("monospaced", Font.PLAIN, 14));
        _edit.setTabSize(4);
        _edit.setLineWrap(true);
        JScrollPane editPane = new JScrollPane(_edit);
        
        getContentPane().add(editPane, BorderLayout.CENTER);
        setPreferredSize(new Dimension(_FRAME_WIDTH, _FRAME_HEIGHT));
        addBindings();
        
        pack();
        setVisible(true);
        
        // start interpreting thread!
        _interpthd.start();        
    }

    private void addToHistory(String cmd) {
        _history.addFirst(cmd);
        if(_history.size() > _HISTORY_SIZE) { 
            _history.removeLast();
        }
    }
    
    //Add a couple of emacs key bindings for navigation.
    private void addBindings() {
        InputMap  inputMap = _edit.getInputMap();
        ActionMap actionMap = _edit.getActionMap();

        //Ctrl-r to start interpret.
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_R, Event.CTRL_MASK), "interpret");
        actionMap.put("interpret", new InterpretAction());

        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_G, Event.CTRL_MASK), "interrupt");
        actionMap.put("interrupt", new InterruptAction());

        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_CLOSE_BRACKET, Event.CTRL_MASK), "Increase log level");
        actionMap.put("Increase log level", new InceaseLogLevelAction());
        
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_OPEN_BRACKET, Event.CTRL_MASK), "Decrease log level");
        actionMap.put("Decrease log level", new DecreaseLogLevelAction());

        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_P, Event.CTRL_MASK), "Previous command");
        actionMap.put("Previous command", new PrevCommandAction());
        
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_N, Event.CTRL_MASK), "Next command");
        actionMap.put("Next command", new NextCommandAction());
    
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_F, Event.CTRL_MASK), "Auto Completion");
        actionMap.put("Auto Completion", new AutoCompleteAction());
    }
    
    
    private native boolean nativeInterpret(String str);
    // interrupt current interpreting.
    private native boolean nativeForceStop();
    private native String  nativeGetLastNativeMessage();
    private native void    nativeSetLogLevel(int lv);
    // <0 : error.
    private native int     nativeAutoComplete(String prefix);
   
    public static void main(String[] args) {
        new Main();
    }
}
