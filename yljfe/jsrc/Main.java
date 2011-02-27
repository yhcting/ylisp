import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.Event;
import java.awt.Font;
import java.awt.event.ActionEvent;
import java.awt.event.KeyEvent;
import java.util.LinkedList;

import javax.swing.AbstractAction;
import javax.swing.ActionMap;
import javax.swing.InputMap;
import javax.swing.JFrame;
import javax.swing.JScrollPane;
import javax.swing.KeyStroke;

public class Main extends JFrame implements SockListener {
	private static boolean _REMOTE = false;

	private static final int _FRAME_WIDTH = 640;
	private static final int _FRAME_HEIGHT = 256;

	// AC : Auto Complete - See native code.
	// already handled at the natives.
	private static final int _AC_HANDLED = 0;

	// there is more possible-common-prefix
	private static final int _AC_MORE_PREFIX = 1;

	// only one candidate exists and is found.
	private static final int _AC_COMPLETE = 2;

	private static final int _HISTORY_SIZE = 100;

	// These log level value should match native ylisp's implementation.
	private static enum _LogLv {
		Verbose(0), Develop(1), Infomation(2), Warn(3), Error(4);

		private int _v;

		_LogLv(int v) {
			_v = v;
		}

		int v() {
			return _v;
		}

		static String getName(int v) {
			for (_LogLv log : _LogLv.values()) {
				if (log.v() == v)
					return log.name();
			}
			return "Unknown";
		}

	}

	private final YLJEditArea _edit;
	private final LinkedList<String> _history = new LinkedList<String>();
	private int _hi = -1; // history index
	private int _loglv = _LogLv.Warn.v(); // default is log ouput - refer
	// native code's implementation
	private String _bufferText;

	// to support remote interpreter daemon
	private Sock _sock;

	// for asynchronous request, response sequence.
	private String _resp_msg = "";
	private final Object _resp_lock = new Object();
	private int _resp_result = 0;

	// =================== To support remote server =====================
	private static final String _CMD_DELIMITER = ":";

	// command string - this should sync. with server side.
	private static final String _CMD_PRINT = "PRINT";
	private static final String _CMD_LOG = "LOG";
	private static final String _CMD_AUTOCOMP_PRINT = "AUTOCOMP_PRINT";
	private static final String _CMD_AUTOCOMP_MORE = "AUTOCOMP_MORE";
	private static final String _CMD_AUTOCOMP_COMP = "AUTOCOMP_COMP";

	public boolean onSocketRead(String msg) {
		int i = msg.indexOf(_CMD_DELIMITER);
		String cmd = msg.substring(0, i);
		String data = msg.substring(i + 1);
		if (cmd.equals(_CMD_PRINT)) {
			System.out.print(data);
		} else if (cmd.equals(_CMD_LOG)) {
			System.out.print(data);
		} else if (cmd.equals(_CMD_AUTOCOMP_PRINT)) {
			synchronized (_resp_lock) {
				_resp_result = _AC_HANDLED;
				_resp_msg = "";
				System.out.print(data);
				_resp_lock.notifyAll();
			}
		} else if (cmd.equals(_CMD_AUTOCOMP_MORE)) {
			synchronized (_resp_lock) {
				_resp_result = _AC_MORE_PREFIX;
				_resp_msg = data;
				_resp_lock.notifyAll();
			}
		} else if (cmd.equals(_CMD_AUTOCOMP_COMP)) {
			synchronized (_resp_lock) {
				_resp_result = _AC_COMPLETE;
				_resp_msg = data;
				_resp_lock.notifyAll();
			}
		} else {
			; // TODO : implement here
		}

		return true;
	}

	private boolean _interpret(String str) {
		if (!_REMOTE)
			return nativeInterpret(str);
		return _sock.send("INTERP" + _CMD_DELIMITER + str);
	}

	private boolean _forceStop() {
		if (!_REMOTE)
			return nativeForceStop();
		// Not implemented yet!
		return true;
	}

	private void _setLogLevel(int lv) {
		if (!_REMOTE)
			nativeSetLogLevel(lv);
		// Not implemented yet!
	}

	private int _autoComplete(String[] more, String prefix) {
		int r = _AC_HANDLED;
		if (!_REMOTE) {
			r = nativeAutoComplete(prefix);
			if (_AC_HANDLED != r)
				more[0] = nativeGetLastNativeMessage();
		} else {
			if (!_sock.send("AUTOCOMP" + _CMD_DELIMITER + prefix)) {
				System.out.println("Fail to send through socket");
				return _AC_HANDLED;
			}
			synchronized (_resp_lock) {
				try {
					_resp_lock.wait();
					r = _resp_result;
				} catch (InterruptedException e) {
					System.out.println("Thread waiting interrupted!");
					r = _AC_HANDLED; // interrupted
				}
			}
			if (_AC_HANDLED != r)
				more[0] = _resp_msg;
		}
		return r;
	}

	// ===================== ACTIONS START ============================
	private class InterpretAction extends AbstractAction {
		public void actionPerformed(ActionEvent ev) {
			new Thread(new Runnable() {
				public void run() {
					String ins = "";
					System.out.print("\n====================== Interpret =====================\n"
				                 	+ _edit.getText() + "\n"
							+ "----------------------------\n\n");
					addToHistory(_edit.getText());
					_hi = -1;
					ins = _edit.getText();
					_edit.setText(""); // clean
					_interpret(ins);
				}
			}).start();
		}
	}

	private class InceaseLogLevelAction extends AbstractAction {
		public void actionPerformed(ActionEvent ev) {
			if (_loglv > _LogLv.Verbose.v()) {
				_loglv--;
				_setLogLevel(_loglv);
				System.out.print(">>> Current Log Level : " + _LogLv.getName(_loglv) + "\n");
			}
		}
	}

	private class DecreaseLogLevelAction extends AbstractAction {
		public void actionPerformed(ActionEvent ev) {
			if (_loglv < _LogLv.Error.v()) {
				_loglv++;
				_setLogLevel(_loglv);
				System.out.print(">>> Current Log Level : " + _LogLv.getName(_loglv) + "\n");
			}
		}
	}

	private class NextCommandAction extends AbstractAction {
		public void actionPerformed(ActionEvent ev) {
			if (_hi > 0) {
				_hi--;
				_edit.setText(_history.get(_hi));
			} else {
				_hi = -1;
				_edit.setText("");
			}
		}
	}

	private class PrevCommandAction extends AbstractAction {
		public void actionPerformed(ActionEvent ev) {
			if (_hi < _history.size() - 1) {
				_hi++;
				_edit.setText(_history.get(_hi));
			} else {
				_hi = _history.size();
				_edit.setText("");
			}
		}
	}

	private class AutoCompleteAction extends AbstractAction {
		public void actionPerformed(ActionEvent ev) {
			int maxi, i, j;
			char[] delimiters = {	' ',
						'\n',
						'\'',
						'\"',
						'(',
						')',
						'\t' };
			String orig = _edit.getText();
			int caretpos = _edit.getCaretPosition();
			String pre = orig.substring(0, caretpos);
			String post = orig.substring(caretpos);

			maxi = -1;
			for (i = 0; i < delimiters.length; i++) {
				j = pre.lastIndexOf(delimiters[i]);
				if (maxi < (j + 1))
					maxi = j + 1;
			}

			int r;
			String more;
			{ // Just scope
				String[] out = new String[1];
				if (0 <= maxi)
					// Normal case
					r = _autoComplete(out,
					                  pre.substring(maxi));
				else
					// In case of first word
					r = _autoComplete(out, pre.substring(0));
				more = out[0];
			}

			switch (r) {
			case _AC_MORE_PREFIX: {
				_edit.setText(pre + more + post);
				_edit.setCaretPosition(caretpos + more.length());
			}
				break;

			case _AC_COMPLETE: {
				more = more + " ";
				_edit.setText(pre + more + post);
				_edit.setCaretPosition(caretpos + more.length());
			}
				break;

			default: // error case
				; // nothing to do
			}
		}
	}

	private class ChangeBufferAction extends AbstractAction {
		public void actionPerformed(ActionEvent ev) {
			String tmp = _edit.getText();
			_edit.setText(_bufferText);
			_bufferText = tmp;
		}
	}

	// ===================== ACTIONS END =======================

	public Main(String[] args) {
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

		// setup socket
		if (_REMOTE) {
			_sock = new Sock();
			try {
				_sock.init(this,
					   args[0],
					   Integer.parseInt(args[1]));
			} catch (NumberFormatException e) {
				System.out.print(e.getMessage());
				System.exit(0);
			}
		}
	}

	private void addToHistory(String cmd) {
		_history.addFirst(cmd);
		if (_history.size() > _HISTORY_SIZE)
			_history.removeLast();
	}

	// Add a couple of emacs key bindings for navigation.
	private void addBindings() {
		InputMap inputMap = _edit.getInputMap();
		ActionMap actionMap = _edit.getActionMap();

		// Ctrl-r to start interpret.
		inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_R,
						    Event.CTRL_MASK),
			     "interpret");
		actionMap.put("interpret", new InterpretAction());

		inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_CLOSE_BRACKET,
						    Event.CTRL_MASK),
			     "Increase log level");
		actionMap.put("Increase log level", new InceaseLogLevelAction());

		inputMap.put(KeyStroke.getKeyStroke(KeyEvent.VK_OPEN_BRACKET,
						    Event.CTRL_MASK),
			     "Decrease log level");
		actionMap.put(	"Decrease log level",
				new DecreaseLogLevelAction());

		inputMap.put(	KeyStroke.getKeyStroke(	KeyEvent.VK_P,
							Event.CTRL_MASK),
				"Previous command");
		actionMap.put("Previous command", new PrevCommandAction());

		inputMap.put(	KeyStroke.getKeyStroke(	KeyEvent.VK_N,
							Event.CTRL_MASK),
				"Next command");
		actionMap.put("Next command", new NextCommandAction());

		inputMap.put(	KeyStroke.getKeyStroke(	KeyEvent.VK_F,
							Event.CTRL_MASK),
				"Auto Completion");
		actionMap.put("Auto Completion", new AutoCompleteAction());

		inputMap.put(	KeyStroke.getKeyStroke(	KeyEvent.VK_B,
							Event.CTRL_MASK),
				"Change Buffer");
		actionMap.put("Change Buffer", new ChangeBufferAction());
	}

	private native boolean nativeInterpret(String str);

	// interrupt current interpreting.
	private native boolean nativeForceStop();

	private native String nativeGetLastNativeMessage();

	private native void nativeSetLogLevel(int lv);

	// <0 : error.
	private native int nativeAutoComplete(String prefix);

	public static void main(String[] args) {
		// remote client mode
		_REMOTE = true;
		uimain(args);
	}

	public static void uimain(String[] args) {
		new Main(args);
	}
}
