package org.julian.splitflap;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothSocket;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.util.Log;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.io.IOException;
import java.io.PrintWriter;

public class MainActivity extends AppCompatActivity {

    Object btLock = new Object();
    BluetoothSocket btSocket = null;
    PrintWriter btWriter = null;
    Thread btThread = null;
    boolean terminated = false;

    TextView statusText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        String[] flaps = {
                "Position 0 (leer)",
                "Position 1 (leer)",
                "Position 2 (leer)",
                "Position 3 (leer)",
                "Position 4 (leer)",
                "Position 5 (leer)",
                "Position 6 (leer)",
                "Position 7 (leer)",
                "ca 5 Minuten später",
                "ca 10 Minuten später",
                "ca 15 Minuten später",
                "ca 20 Minuten später",
                "ca 25 Minuten später",
                "ca 30 Minuten später",
                "ca 40 Minuten später",
                "ca 50 Minuten später",
                "ca 60 Minuten später",
                "über 60 Minuten später",
                "unbestimmt verspätet",
                "Zug fällt aus",
                "Der Zuganzeiger ist gestört",
                "heute von Gleis 1",
                "heute von Gleis 2",
                "heute von Gleis 3",
                "heute von Gleis 4",
                "heute von Gleis 5",
                "heute von Gleis 6",
                "heute von Gleis 8",
                "heute von Gleis 9",
                "heute von Gleis 10",
                "heute von Gleis 11",
                "heute von Gleis 12",
                "heute von Gleis 13",
                "Position 33 (leer)",
                "Position 34 (leer)",
                "Position 35 (leer)",
                "Position 36 (leer)",
                "Position 37 (leer)",
                "Position 38 (leer)",
                "Position 39 (leer)"
        };

        ArrayAdapter<String> flaplistAdapter = new ArrayAdapter<String>(this, R.layout.list_item_flaplist, R.id.list_item_flaplist_textview, flaps);
        ListView flaplist = (ListView) findViewById(R.id.flaplist);
        flaplist.setAdapter(flaplistAdapter);

        statusText = (TextView) findViewById(R.id.statusText);

        flaplist.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view,
                                    int position, long id) {
                sendCommandBT("FLAP " + position + "\n");
            }
        });


        findViewById(R.id.rebootButton).setOnClickListener(new AdapterView.OnClickListener() {
            @Override
            public void onClick(View view) {
                sendCommandBT("REBOOT\n");
            }
        });

        findViewById(R.id.connectButton).setOnClickListener(new AdapterView.OnClickListener() {
            @Override
            public void onClick(View view) {
                connectBT();
            }
        });

        findViewById(R.id.disconnectButton).setOnClickListener(new AdapterView.OnClickListener() {
            @Override
            public void onClick(View view) {
                disconnectBT();
            }
        });

        btThread = new Thread(new Runnable() {
            public void run() {

                while(!terminated) {

                    synchronized (btLock) {
                        if (isConnectedBT()) {
                            try {
                                btWriter.write("NOP\n");
                                Log.i("check alive", "alive");
                            } catch (Exception e) {
                                Log.i("check alive", "not alive");
                                disconnectBT();
                            }
                        }
                    }

                    try {
                        for (int i = 0; i < 200 && !terminated; i++) {
                            Thread.sleep(5);
                        }
                    } catch (InterruptedException e) {
                        // do nothing
                    }
                }

            }
        });

        btThread.start();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        terminated = true;

        disconnectBT();
    }

    private void connectBT() {
        if (isConnectedBT()) {
            return;
        }

        BluetoothAdapter blueAdapter = BluetoothAdapter.getDefaultAdapter();
        if (blueAdapter != null) {
            if (blueAdapter.isEnabled()) {
                BluetoothDevice splitflapController = null;

                for (BluetoothDevice dev : blueAdapter.getBondedDevices()) {
                    if (dev.getName().equals("Splitflap")) {
                        splitflapController = dev;
                        break;
                    }
                }

                if (splitflapController != null) {
                    try {
                        synchronized (btLock) {
                            btSocket = splitflapController.createRfcommSocketToServiceRecord(splitflapController.getUuids()[0].getUuid());
                            btSocket.connect();
                            btWriter = new PrintWriter(btSocket.getOutputStream());

                            statusText.setText("Verbunden");
                        }
                    } catch (IOException e) {
                        Toast.makeText(getApplicationContext(),
                                "Fehler beim Verbinden: " + e.getMessage(), Toast.LENGTH_LONG)
                                .show();
                    }
                } else {
                    Toast.makeText(getApplicationContext(),
                            "Keine Fallblattanzeige gefunden", Toast.LENGTH_LONG)
                            .show();
                }
            } else {
                Toast.makeText(getApplicationContext(),
                        "Bluetooth nicht aktiviert", Toast.LENGTH_LONG)
                        .show();
            }
        } else {
            Toast.makeText(getApplicationContext(),
                    "Bluetooth nicht vorhanden", Toast.LENGTH_LONG)
                    .show();
        }
    }

    private void disconnectBT() {
        if (isConnectedBT()) {
            try {
                btWriter.close();
                btSocket.close();
            } catch (IOException e) {
                //do nothing
            }

            statusText.setText("Nicht Verbunden");
        }
    }

    private boolean isConnectedBT() {
        return btSocket != null && btSocket.isConnected();
    }

    private void sendCommandBT(String command) {
        synchronized (btLock) {
            if (!isConnectedBT()) {
                Toast.makeText(getApplicationContext(),
                        "Bluetooth nicht verbunden", Toast.LENGTH_LONG)
                        .show();
                return;
            }

            btWriter.print(command);
            btWriter.flush();
        }
    }


}
