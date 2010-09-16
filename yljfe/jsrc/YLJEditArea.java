import java.awt.event.KeyEvent;

import javax.swing.JTextArea;

class YLJEditArea extends JTextArea {
    
    YLJEditArea() {
        super(1, 1);
    }
    
    /**
     * Assume.
     *   last added char is right parenthesis!
     * @param rpos
     * @return: -1 means "Cannot find matching one"
     */
    private int _findMatchingLeftParenth(int pos) {
        int    cnt = 1; // one more right parenthesis
        int    i;
        String t = getText();
        i = pos-1;
        for(;i>=0; i--) {
            if('(' == t.charAt(i)) {
                cnt--;
            } else if(')' == t.charAt(i)) {
                cnt++;
            }
            if(0 == cnt) {
                /* matching left is found! */
                return i;
            }
        }
        return -1;
    }
    
    protected void processKeyEvent(KeyEvent e) {
        if(')' == e.getKeyChar()
                && KeyEvent.KEY_TYPED == e.getID()) { 
            int    pos = getCaretPosition();
            int    m = _findMatchingLeftParenth(pos);
            if(m >= 0) {
                moveCaretPosition(m);
                paintImmediately(getBounds(null));
                setCaretPosition(pos);
                try {
                    Thread.sleep(250);
                } catch (InterruptedException ex){
                    ; //nothing to do!
                }
            }
        }
        super.processKeyEvent(e);
    }
}