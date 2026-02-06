const express = require('express');
const http = require('http');
const { Server } = require('socket.io');
const path = require('path');
const fs = require('fs');

const app = express();
const server = http.createServer(app);
const io = new Server(server, {
    cors: { origin: "*" }
});

const DB_FILE = './database.json';

app.use(express.json()); 

// ngrok skip warning
app.use((req, res, next) => {
    res.setHeader('ngrok-skip-browser-warning', 'true');
    next();
});

// --- දත්ත ගබඩාව පරීක්ෂා කිරීම සහ මූලික සැකසුම් ---
if (!fs.existsSync(DB_FILE)) {
    const initialData = {
        schedule: { start: "07:30", end: "13:30" },
        devices: [
            { 
                id: "dev1", 
                sn: 1, 
                name: "Smart Board 01", 
                location: "Main Hall", 
                classNo: "10-A", 
                dept: "General", 
                hours: 0, 
                power: "OFF", 
                current: 0, 
                alarmOn: true, 
                lastSeen: "Never" 
            }
        ],
        history: [], 
        users: [{ id: "50689", pass: "Ramesh@21", role: "admin" }]
    };
    fs.writeFileSync(DB_FILE, JSON.stringify(initialData, null, 2));
}

function readDB() { return JSON.parse(fs.readFileSync(DB_FILE)); }
function saveDB(data) { fs.writeFileSync(DB_FILE, JSON.stringify(data, null, 2)); }

function getSLDateTime() {
    return new Date().toLocaleString('en-GB', { timeZone: 'Asia/Colombo' });
}

// Log එකතු කිරීම (Activity Logs සඳහා)
function addLog(msg) {
    let db = readDB();
    if(!db.history) db.history = [];
    db.history.unshift({ 
        time: getSLDateTime(), 
        event: msg,
        type: "system" 
    });
    if (db.history.length > 1000) db.history.pop(); 
    saveDB(db);
}

app.use(express.static(path.join(__dirname, 'public')));

// --- ESP8266 Heartbeat API ---
app.post('/api/register', (req, res) => {
    const { id, current } = req.body; 
    let db = readDB();
    let dev = db.devices.find(d => d.id === id);
    const currentTime = getSLDateTime();

    if (!dev) {
        dev = {
            id: id,
            sn: db.devices.length + 1,
            name: "Smart Board " + id,
            location: "Auto Detected",
            classNo: "N/A",
            dept: "General",
            hours: 0,
            power: "OFF",
            current: current || 0,
            alarmOn: true,
            lastSeen: currentTime
        };
        db.devices.push(dev);
        addLog(`New device auto-discovered: ${id}`);
    } else {
        dev.lastSeen = currentTime;
        dev.current = current || 0;
    }
    
    saveDB(db);
    io.emit('sync', db); 
    res.status(200).json({ status: "Success", power: dev.power });
});

// --- SOCKET.IO ---
io.on('connection', (socket) => {
    console.log('Client Connected: ' + socket.id);
    socket.emit('sync', readDB());

    // 1. ON/OFF පාලනය සහ History එකතු කිරීම (Report එක සඳහා)
    socket.on('control', (data) => {
        let db = readDB();
        let dev = db.devices.find(d => d.id === data.id);
        if (dev) {
            dev.power = data.cmd;
            
            // රිපෝට් එක සඳහා වඩාත් පැහැදිලි History එකක් එකතු කිරීම
            const historyEntry = {
                time: getSLDateTime(),
                name: dev.name,
                location: dev.location || "N/A",
                classNo: dev.classNo || "N/A",
                action: data.cmd,
                event: `${dev.name} (${dev.classNo}) turned ${data.cmd}`,
                type: "control"
            };

            if(!db.history) db.history = [];
            db.history.unshift(historyEntry);
            
            saveDB(db);
            io.emit('control', { id: data.id, cmd: data.cmd }); 
            io.emit('sync', db);
        }
    });

    // 2. Settings සංස්කරණය
    socket.on('saveEdit', (data) => {
        let db = readDB();
        let dev = db.devices.find(d => d.id === data.id);
        if (dev) {
            dev.name = data.name;
            dev.location = data.location || dev.location;
            dev.classNo = data.classNo || dev.classNo;
            dev.dept = data.dept || dev.dept;
            dev.hours = parseInt(data.hours) || 0;
            
            saveDB(db);
            addLog(`Settings updated for ${dev.name} (${dev.classNo})`);
            io.emit('sync', db);
        }
    });

    // 3. අලුතින් Device එකක් UI එකෙන් එකතු කිරීම (Arduino නැතිව Test කිරීමට)
    socket.on('addDevice', (data) => {
        let db = readDB();
        const newDev = {
            id: data.id || "manual-" + Date.now(),
            sn: db.devices.length + 1,
            name: data.name,
            location: data.location,
            classNo: data.classNo || "N/A",
            dept: "General",
            hours: 0,
            power: "OFF",
            current: 0,
            alarmOn: true,
            lastSeen: getSLDateTime()
        };
        db.devices.push(newDev);
        saveDB(db);
        addLog(`Device added manually: ${data.name}`);
        io.emit('sync', db);
    });

    socket.on('updateSchedule', (data) => {
        let db = readDB();
        db.schedule = data;
        saveDB(db);
        addLog(`Schedule updated: ${data.start} - ${data.end}`);
        io.emit('sync', db);
    });

    socket.on('alarmToggle', (data) => {
        let db = readDB();
        let dev = db.devices.find(d => d.id === data.id);
        if (dev) {
            dev.alarmOn = data.state;
            saveDB(db);
            io.emit('sync', db);
        }
    });

    socket.on('addNewUser', (data) => {
        let db = readDB();
        if (!db.users) db.users = [];
        db.users.push(data);
        saveDB(db);
        addLog(`New user added: ${data.id}`);
        io.emit('sync', db); // User list එකත් sync කරන්න
    });

    // 4. Logs Clear කිරීමේ පහසුකම
    socket.on('clearLogs', () => {
        let db = readDB();
        db.history = [];
        saveDB(db);
        addLog("All system logs cleared by admin.");
        io.emit('sync', db);
    });

    socket.on('disconnect', () => {
        console.log('Client Disconnected');
    });
});

const PORT = 3000;
server.listen(PORT, '0.0.0.0', () => {
    console.log(`\n==================================================`);
    console.log(`🚀 SMARTBOARD SERVER v2.6 - READY`);
    console.log(`📊 History Logging: Advanced (Report Optimized)`);
    console.log(`🔗 URL: http://localhost:${PORT}`);
    console.log(`==================================================\n`);
});