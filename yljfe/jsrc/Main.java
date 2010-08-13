import java.awt.*;
import java.awt.event.*;
import javax.swing.*;
import java.util.*;

public class Main extends JFrame {
    private static final int       _FRAME_WIDTH        = 1024;
    private static final int       _FRAME_HEIGHT       = 1024;
    private static final int       _EDIT_AREA_HEIGHT   = 256;
    private static final int       _OUT_AREA_HEIGHT    = _FRAME_HEIGHT - _EDIT_AREA_HEIGHT;
    
    // AC : Auto Complete - See native code.
    private static final int       _AC_WRONG_PREFIX    = 0; // there is no matching candidates. May be wrong prefix. 
    private static final int       _AC_CANDIDATES      = 1;
    private static final int       _AC_MORE_PREFIX     = 2; // there is more possible-common-prefix
    private static final int       _AC_COMPLETE        = 3; // only one candidate exists and is found.
    
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
    private JTextArea           _out;
    private LinkedList<String>  _history = new LinkedList<String>();
    private int                 _hi = 0;  // history index
    private int                 _loglv = _LogLv.Warn.v(); // default is log ouput - refer native code's implementation

    // ============================= ACTIONS START ==============================
    private class InterpretAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            _out.setText("====================== Interpret =====================\n" +
                         _edit.getText() + "\n" +
                         "----------------------------\n\n");
            
            nativeInterpret(_edit.getText());
            addToHistory(_edit.getText());
            _hi = 0;
            _edit.setText(""); // clean
            _out.append(nativeGetLastNativeMessage() + "\n");
            _out.append("======================================================\n");
        }
    }

    private class CleanOutputAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            _out.setText(""); // clean
        }
    }

    private class InceaseLogLevelAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_loglv > _LogLv.Verbose.v()) { 
                _loglv--;
                nativeSetLogLevel(_loglv);
                _out.append(">>> Current Log Level : " + _LogLv.getName(_loglv) + "\n");
            }
        }
    }
    
    private class DecreaseLogLevelAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_loglv < _LogLv.Error.v()) { 
                _loglv++;
                nativeSetLogLevel(_loglv);
                _out.append(">>> Current Log Level : " + _LogLv.getName(_loglv) + "\n");
            }
        }
    }
    
    private class NextCommandAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_hi > 0) {
                _hi--;
                _edit.setText((String)_history.get(_hi));
            } else {
                _edit.setText("");
            }
        }
    }

    private class PrevCommandAction extends AbstractAction {
        public void actionPerformed(ActionEvent ev) {
            if(_hi < _history.size()) {
                _edit.setText((String)_history.get(_hi));
                _hi++;
            } else {
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
            
            _out.setText(""); // clean output screen fist.
            
            maxi = 0;
            for(i=0; i<delimiters.length; i++) {
                j = pre.lastIndexOf(delimiters[i]);
                if(maxi<j) { maxi = j; }
            }
            
            if( maxi < (pre.length() - 1) ) {
                int r = nativeAutoComplete(pre.substring(maxi+1));
                switch(r) {
                    case _AC_WRONG_PREFIX:
                        ; // nothing to do
                    break;
                    
                    case _AC_CANDIDATES: {
                        _out.setText(nativeGetLastNativeMessage());
                    } break;
                    
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
            }
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
        
        _out = new JTextArea(1, 1);
        _out.setFont(new Font("monospaced", Font.PLAIN, 14));
        _out.setEditable(false);
        _out.setTabSize(4);
        _out.setLineWrap(true);
        JScrollPane outPane = new JScrollPane(_out);
        
        JSplitPane splitPane = new JSplitPane(JSplitPane.VERTICAL_SPLIT,
                                              outPane, editPane);
        splitPane.setPreferredSize(new Dimension(_FRAME_WIDTH, _FRAME_HEIGHT));
        splitPane.setOneTouchExpandable(true);
        splitPane.setDividerLocation(_FRAME_HEIGHT*9/10); /* about 80% */
        
        JPanel content = new JPanel();
        content.setLayout(new BorderLayout());
        
        getContentPane().add(splitPane, BorderLayout.CENTER);
        
        addBindings();
        
        pack();
        setVisible(true);
        
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

        //Ctrl-l to clean output text screen.
        inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_L, Event.CTRL_MASK), "clean_output");
        actionMap.put("clean_output", new CleanOutputAction());
        
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
    private native String  nativeGetLastNativeMessage();
    private native void    nativeSetLogLevel(int lv);
    // <0 : error.
    private native int     nativeAutoComplete(String prefix);
   
    public static void main(String[] args) {
        new Main();
    }
}
