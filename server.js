const express = require("express");
const admin = require("firebase-admin");
const bodyParser = require("body-parser");
const cors = require("cors");

// Load Firebase Credentials
const serviceAccount = require("./serviceAccountKey.json");

// Initialize Firebase
admin.initializeApp({
  credential: admin.credential.cert(serviceAccount),
  databaseURL: "https://sammuel-17249-default-rtdb.firebaseio.com",
});

const db = admin.database();
const ref = db.ref("scannedData");

const app = express();
app.use(cors()); // Allow cross-origin requests (useful for web apps)
app.use(bodyParser.json()); // Parse JSON body

// ✅ **Route to receive QR data from Arduino**
app.post("/send", (req, res) => {
  const { qrCode } = req.body;

  if (!qrCode) {
    return res.status(400).json({ error: "QR Code is required" });
  }

  const newEntry = ref.push();
  newEntry
    .set({
      qrCode: qrCode,
      timestamp: new Date().toISOString(),
    })
    .then(() => res.json({ success: true, message: "QR Code stored successfully!" }))
    .catch((error) => res.status(500).json({ error: error.message }));
});

// ✅ **Route to retrieve all stored QR data**
app.get("/get", (req, res) => {
  ref.once("value", (snapshot) => {
    const data = snapshot.val();
    res.json(data || {});
  });
});

// ✅ **Server listening on port 3000**
const PORT = 3000;
app.listen(PORT, () => {
  console.log(`🚀 Server running on http://localhost:${PORT}`);
});
