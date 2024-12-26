#include <WiFi.h>
#include<time.h>
#include <FS.h>
#include <WebServer.h>
#include <Wire.h>                    
#include <LiquidCrystal_I2C.h> 
#include <Preferences.h>
#include <Adafruit_Fingerprint.h>
#include <SoftwareSerial.h>
#include <SPIFFS.h>
#include <Keypad.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <set>
#include <map>

#define MAX_STUDENTS 100  // Adjust this based on the maximum number of students


bool attendedStudentsarray[MAX_STUDENTS] = {false};  // Array to track attendance
bool isAttendanceMode = false;
std::vector<String> parseCSVLine(const String& line, const std::vector<int>& columns) {
    std::vector<String> result;
    int columnIndex = 0;
    int startIndex = 0;
    int foundIndex = line.indexOf(',');

    while (foundIndex != -1) {
        if (std::find(columns.begin(), columns.end(), columnIndex) != columns.end()) {
            result.push_back(line.substring(startIndex, foundIndex));
        }
        columnIndex++;
        startIndex = foundIndex + 1;
        foundIndex = line.indexOf(',', startIndex);
    }

    // Check last column after the last comma
    if (std::find(columns.begin(), columns.end(), columnIndex) != columns.end()) {
        result.push_back(line.substring(startIndex));
    }

    return result;
}


LiquidCrystal_I2C lcd(0x27, 16, 2);  // 16 columns, 2 rows, address is typically 0x27
Preferences preferences;
// Define keypad size (rows and columns)
const byte ROWS = 4; // Four rows
const byte COLS = 4; // Four columns

// Key mapping
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};

// Connect keypad rows and columns to GPIO pins on the ESP32S
byte rowPins[ROWS] = {18, 19, 25, 26}; // Adjust as per your wiring
byte colPins[COLS] = {23, 5, 4, 15};   // Adjust as per your wiring

// Create a Keypad object
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);


#define RX_PIN 16
#define TX_PIN 17

SoftwareSerial mySerial(RX_PIN, TX_PIN);
Adafruit_Fingerprint finger(&mySerial);
WebServer server(80);


String ssid = "";
String password = "";
String coursecode = "";

const char* adminUsername = "admin";
const char* adminPassword = "admin123";
const char* teacherDataFile = "/teachers.csv";
const char* studentDataFile = "/students.csv";
const char* attendanceDataFile = "/attendance.csv";
const char* SUPABASE_URL = "YOUR_SUPABASE_URL";
const char* SUPABASE_KEY = "YOUR_SUPABASE_ANON_KEY";
void handleEditTeacher();
void handleEditStudent();
void handleEditAttendance();
void handleUpdateTeacher();
void handleUpdateStudent();
void handleDeleteTeacher();
void handleDeleteStudent();
void handleDeleteAttendanceRecord();
void handleAttendanceDetails();
void handleAttendanceDetails1();
void handleDateWiseDownload();
void handleDownloadAttendanceByDate();
void handleDownloadAttendance25();
void handleUpdateAttendance();
void handleDeleteAttendance25();
void handleDownloadAttendance();
void handleDownloadDateAttendance();


void handleEditTeacher() {
  if (!authenticateUser()) {
    return;
  }

  String htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <style>
            table {
                width: 100%;
                border-collapse: collapse;
                margin-bottom: 20px;
            }
            th, td {
                border: 1px solid #ddd;
                padding: 8px;
                text-align: left;
            }
            th {
                background-color: #4CAF50;
                color: white;
            }
            .edit-btn {
                background-color: #4CAF50;
                color: white;
                padding: 5px 10px;
                border: none;
                cursor: pointer;
            }
            .delete-btn {
                background-color: #f44336;
                color: white;
                padding: 5px 10px;
                border: none;
                cursor: pointer;
            }
        </style>
    </head>
    <body>
        <h2>Edit Teachers</h2>
        <table>
            <tr>
                <th>ID</th>
                <th>Name</th>
                <th>Course Code</th>
                <th>Actions</th>
            </tr>
    )rawliteral";

  File file = SPIFFS.open(teacherDataFile, "r");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int firstComma = line.indexOf(',');
      int lastComma = line.lastIndexOf(',');
      String id = line.substring(0, firstComma);
      String name = line.substring(firstComma + 1, lastComma);
      String courseCode = line.substring(lastComma + 1);

      htmlPage += "<tr><td>" + id + "</td><td>" + name + "</td><td>" + courseCode + 
                 "</td><td><button class='edit-btn' onclick='editTeacher(\"" + id + "\", \"" + name + "\", \"" + courseCode + 
                 "\")'>Edit</button> <button class='delete-btn' onclick='deleteTeacher(\"" + id + 
                 "\")'>Delete</button></td></tr>";
    }
  }
  file.close();

  htmlPage += R"rawliteral(
        </table>
        <script>
            function editTeacher(id, name, courseCode) {
                let newName = prompt("Enter new name:", name);
                let newCourseCode = prompt("Enter new course code:", courseCode);
                if (newName && newCourseCode) {
                    window.location.href = `/updateTeacher?id=${id}&name=${newName}&courseCode=${newCourseCode}`;
                }
            }
            function deleteTeacher(id) {
                if (confirm("Are you sure you want to delete this teacher?")) {
                    window.location.href = `/deleteTeacher?id=${id}`;
                }
            }
        </script>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}

void handleUpdateTeacher() {
  if (!authenticateUser()) {
    return;
  }

  String id = server.arg("id");
  String newName = server.arg("name");
  String newCourseCode = server.arg("courseCode");

  File oldFile = SPIFFS.open(teacherDataFile, "r");
  File newFile = SPIFFS.open("/temp.csv", "w");

  while (oldFile.available()) {
    String line = oldFile.readStringUntil('\n');
    if (line.startsWith(id + ",")) {
      newFile.println(id + "," + newName + "," + newCourseCode);
    } else {
      newFile.println(line);
    }
  }

  oldFile.close();
  newFile.close();

  SPIFFS.remove(teacherDataFile);
  SPIFFS.rename("/temp.csv", teacherDataFile);

  server.sendHeader("Location", "/editTeacher");
  server.send(303);
}

void handleDeleteTeacher() {
  if (!authenticateUser()) {
    return;
  }

  String id = server.arg("id");
  File oldFile = SPIFFS.open(teacherDataFile, "r");
  File newFile = SPIFFS.open("/temp.csv", "w");

  while (oldFile.available()) {
    String line = oldFile.readStringUntil('\n');
    if (!line.startsWith(id + ",")) {
      newFile.println(line);
    }
  }

  oldFile.close();
  newFile.close();

  SPIFFS.remove(teacherDataFile);
  SPIFFS.rename("/temp.csv", teacherDataFile);

  server.sendHeader("Location", "/editTeacher");
  server.send(303);
}

// Similar functions for students
void handleEditStudent() {
  if (!authenticateUser()) {
    return;
  }

  String htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <style>
            table {
                width: 100%;
                border-collapse: collapse;
                margin-bottom: 20px;
            }
            th, td {
                border: 1px solid #ddd;
                padding: 8px;
                text-align: left;
            }
            th {
                background-color: #4CAF50;
                color: white;
            }
            .edit-btn {
                background-color: #4CAF50;
                color: white;
                padding: 5px 10px;
                border: none;
                cursor: pointer;
            }
            .delete-btn {
                background-color: #f44336;
                color: white;
                padding: 5px 10px;
                border: none;
                cursor: pointer;
            }
        </style>
    </head>
    <body>
        <h2>Edit Students</h2>
        <table>
            <tr>
                <th>ID</th>
                <th>Name</th>
                <th>Roll Number</th>
                <th>Actions</th>
            </tr>
    )rawliteral";

  File file = SPIFFS.open(studentDataFile, "r");
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      int firstComma = line.indexOf(',');
      int lastComma = line.lastIndexOf(',');
      String id = line.substring(0, firstComma);
      String name = line.substring(firstComma + 1, lastComma);
      String rollNumber = line.substring(lastComma + 1);

      htmlPage += "<tr><td>" + id + "</td><td>" + name + "</td><td>" + rollNumber + 
                 "</td><td><button class='edit-btn' onclick='editStudent(\"" + id + "\", \"" + name + "\", \"" + rollNumber + 
                 "\")'>Edit</button> <button class='delete-btn' onclick='deleteStudent(\"" + id + 
                 "\")'>Delete</button></td></tr>";
    }
  }
  file.close();

  htmlPage += R"rawliteral(
        </table>
        <script>
            function editStudent(id, name, rollNumber) {
                let newName = prompt("Enter new name:", name);
                let newRollNumber = prompt("Enter new roll number:", rollNumber);
                if (newName && newRollNumber) {
                    window.location.href = `/updateStudent?id=${id}&name=${newName}&rollNumber=${newRollNumber}`;
                }
            }
            function deleteStudent(id) {
                if (confirm("Are you sure you want to delete this student?")) {
                    window.location.href = `/deleteStudent?id=${id}`;
                }
            }
        </script>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}

void handleUpdateStudent() {
  if (!authenticateUser()) {
    return;
  }

  String id = server.arg("id");
  String newName = server.arg("name");
  String newRollNumber = server.arg("rollNumber");

  File oldFile = SPIFFS.open(studentDataFile, "r");
  File newFile = SPIFFS.open("/temp.csv", "w");

  while (oldFile.available()) {
    String line = oldFile.readStringUntil('\n');
    if (line.startsWith(id + ",")) {
      newFile.println(id + "," + newName + "," + newRollNumber);
    } else {
      newFile.println(line);
    }
  }

  oldFile.close();
  newFile.close();

  SPIFFS.remove(studentDataFile);
  SPIFFS.rename("/temp.csv", studentDataFile);

  server.sendHeader("Location", "/editStudent");
  server.send(303);
}

void handleDeleteStudent() {
  if (!authenticateUser()) {
    return;
  }

  String id = server.arg("id");
  File oldFile = SPIFFS.open(studentDataFile, "r");
  File newFile = SPIFFS.open("/temp.csv", "w");

  while (oldFile.available()) {
    String line = oldFile.readStringUntil('\n');
    if (!line.startsWith(id + ",")) {
      newFile.println(line);
    }
  }

  oldFile.close();
  newFile.close();

  SPIFFS.remove(studentDataFile);
  SPIFFS.rename("/temp.csv", studentDataFile);

  server.sendHeader("Location", "/editStudent");
  server.send(303);
}




void handleDeleteAttendanceRecord() {
  if (!authenticateUser()) {
    return;
  }

  String courseCode = server.arg("courseCode");
  String studentId = server.arg("studentId");
  String timestamp = server.arg("timestamp");

  File oldFile = SPIFFS.open("/attendance.csv", "r");
  File newFile = SPIFFS.open("/temp.csv", "w");

  while (oldFile.available()) {
    String line = oldFile.readStringUntil('\n');
    if (!line.startsWith(courseCode + "," + studentId + "," + timestamp)) {
      newFile.println(line);
    }
  }

  oldFile.close();
  newFile.close();

  SPIFFS.remove("/attendance.csv");
  SPIFFS.rename("/temp.csv", "/attendance.csv");

  server.sendHeader("Location", "/editAttendance");
  server.send(303);
}
bool initializeFingerprint() {
    mySerial.begin(57600);
    delay(100);
    finger.begin(57600);
    delay(100);
    
    int retry = 0;
    while (!finger.verifyPassword()) {
        if (retry >= 3) {
            Serial.println("Fingerprint sensor not detected or communication error!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Sensor Error!");
            lcd.setCursor(0, 1);
            lcd.print("Check Connection");
            return false;
        }
        Serial.println("Retrying sensor connection...");
        delay(1000);
        retry++;
    }
    
    Serial.println("Fingerprint sensor initialized successfully!");
    return true;
}

bool sendToSupabase(const char* table, const String& jsonData) {
  HTTPClient http;
  String url = String(SUPABASE_URL) + "/rest/v1/" + table;
  
  http.begin(url);
  http.addHeader("apikey", SUPABASE_KEY);
  http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Prefer", "return=minimal");
  
  int httpCode = http.POST(jsonData);
  bool success = (httpCode == 201);
  http.end();
  
  return success;
}

// Connect to Wi-Fi
bool connectToWiFi(const char* ssid, const char* password) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting...");
  Serial.print("Connecting to Wi-Fi");

  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    timeout++;
    if (timeout > 20) {
      Serial.println("\nFailed to connect to Wi-Fi");
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Wi-Fi Failed");
      delay(2000);
      return false;
    }
  }

  Serial.println("\nConnected to Wi-Fi!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connected");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP().toString());

  return true;
}

// Start Wi-Fi in AP mode
void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ESP32_Config", "123456789");
  Serial.println("Access Point Started");
  Serial.print("AP IP Address: ");
  Serial.println(WiFi.softAPIP());
}

// Authenticate User
bool authenticateUser() {
  if (!server.authenticate(adminUsername, adminPassword)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

// Serve the root page after login
void handleRoot() {
  if (!authenticateUser()) {
    return;
  }

  String wifiStatus = "Not connected";
  if (WiFi.status() == WL_CONNECTED) {
    wifiStatus = WiFi.SSID();
  }

  String htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
     <body>
     
    <div class="dashboard-container">
        <h2 class="dashboard-title">Admin Dashboard</h2>
        <p class="wifi-status">Current Wi-Fi Status: )rawliteral" + wifiStatus + R"rawliteral(</p>
        
        <div class="menu-container">
            <a href="/wifi" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
                <i class="icon">📡</i>Change Wi-Fi Settings
            </a>
            
            <a href="/enrollTeacher" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
                <i class="icon">👨‍🏫</i>Teacher Enrollment
            </a>
            
            <a href="/downloadTeachers" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
                <i class="icon">📥</i>Download Teacher Data
            </a>
            
            <a href="/downloadStudents" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
                <i class="icon">📚</i>Download Student Data
            </a>
            <a href="/attendanceDetails?courseCode=default" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
    <i class="icon">📊</i>View Attendance Details
</a>
            
            <a href="/enrollStudent" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
                <i class="icon">👨‍🎓</i>Student Enrollment
            </a>
            
            <a href="/attendancemode" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
                <i class="icon">✅</i>Attendance Mode
            </a>
            
            <a href="/downloadAttendance" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
                <i class="icon">📊</i>View Attendance
            </a>
            <a href="/editTeacher" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
    <i class="icon">✏️</i>Edit Teachers
    </a>

    <a href="/editStudent" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
    <i class="icon">✏️</i>Edit Students
     </a>

    <a href="/editAttendance" class="menu-button" onmouseover="this.style.transform='scale(1.05)'" onmouseout="this.style.transform='scale(1)'">
    <i class="icon">✏️</i>Edit Attendance
    </a>
        </div>
        
        <a href="/deleteAllFiles" 
           class="delete-button"
           onmouseover="this.style.backgroundColor='#ff6b6b'" 
           onmouseout="this.style.backgroundColor='#ff4444'"
           onclick="return confirm('Warning: This action will permanently delete all CSV files. Are you sure you want to proceed?')">
            <i class="icon">🗑️</i>Delete All CSV Files
        </a>
    </div>
    
    <style>
        .dashboard-container {
            padding: 20px;
            max-width: 600px;
            margin: 0 auto;
        }
        
        .dashboard-title {
            color: #2c3e50;
            text-align: center;
            margin-bottom: 20px;
        }
        
        .wifi-status {
            background-color: transparent;
            padding: 10px;
            border-radius: 5px;
            text-align: center;
            margin-bottom: 20px;
            border: 1px solid rgba(0, 0, 0, 0.1);
        }
        
        .menu-container {
            display: flex;
            flex-direction: column;
            gap: 10px;
        }
        
        .menu-button {
            display: flex;
            align-items: center;
            padding: 12px 20px;
            background-color: rgba(76, 175, 80, 0.9);
            color: white;
            text-decoration: none;
            border-radius: 8px;
            transition: all 0.3s ease;
        }
        
        .menu-button:hover {
            background-color: rgba(69, 160, 73, 0.9);
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
        }
        
        .icon {
            margin-right: 10px;
            font-style: normal;
        }
        
        .delete-button {
            display: flex;
            align-items: center;
            justify-content: center;
            margin-top: 20px;
            padding: 12px 20px;
            background-color: rgba(255, 68, 68, 0.9);
            color: white;
            text-decoration: none;
            border-radius: 8px;
            transition: all 0.3s ease;
        }
        
        .delete-button:hover {
            box-shadow: 0 4px 8px rgba(255,0,0,0.2);
        }
        
        @media (max-width: 480px) {
            .dashboard-container {
                padding: 10px;
            }
            
            .menu-button, .delete-button {
                padding: 10px 15px;
            }
        }
    </style>
      </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}

// Serve the Wi-Fi settings page
void handleWifiSettings() {
  if (!authenticateUser()) {
    return;
  }

  String htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <body>
        <h2>Change Wi-Fi Settings</h2>
        <form action="/saveWifi" method="POST">
          <label for="ssid">SSID:</label><br>
          <input type="text" id="ssid" name="ssid" value=")rawliteral" + ssid + R"rawliteral("><br><br>
          <label for="password">Password:</label><br>
          <input type="password" id="password" name="password" value=")rawliteral" + password + R"rawliteral("><br><br>
          <input type="submit" value="Save Wi-Fi Settings">
        </form>
      </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}

// Save new Wi-Fi credentials
void handleSaveWifi() {
  if (!server.hasArg("ssid") || !server.hasArg("password")) {
    server.send(400, "text/html", "Missing SSID or password");
    return;
  }

  ssid = server.arg("ssid");
  password = server.arg("password");

  preferences.begin("WiFi", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();

  // Connect to new Wi-Fi
  if (connectToWiFi(ssid.c_str(), password.c_str())) {
    server.send(200, "text/html", "<h2>Wi-Fi settings saved. Rebooting...</h2>");
    delay(2000);
    ESP.restart();
  } else {
    server.send(500, "text/html", "<h2>Failed to connect to Wi-Fi.</h2>");
  }
}

// Teacher enrollment page
void handleEnrollTeacher() {
  if (!authenticateUser()) {
    return;
  }

  String htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <body>
        <h2>Teacher Enrollment</h2>
        <form action="/enrollTeacherSubmit" method="POST">
          <label for="teacherId">Teacher ID (SL):</label><br>
          <input type="text" id="teacherId" name="teacherId"><br><br>
          <label for="teacherName">Teacher Name:</label><br>
          <input type="text" id="teacherName" name="teacherName"><br><br>
          <label for="coursecode">Course Code:</label><br>
          <input type="text" id="coursecode" name="coursecode"><br><br>
          <input type="submit" value="Enroll Teacher">
        </form>
      </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}

// Enroll a teacher and take attendance
void enrollTeacher(int teacherId, const String& teacherName, const String& coursecode) {
    if (!initializeFingerprint()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sensor Error!");
        lcd.setCursor(0, 1);
        lcd.print("Try Again Later");
        delay(3000);
        return;
    }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enrol Teacher ID:");
  
  delay(2000);
  Serial.print("Starting enrollment for Teacher ID: ");
  Serial.println(teacherId);

  // Directly initialize the fingerprint sensor here
  mySerial.begin(57600);
  delay(1000);
  finger.begin(57600);

  int status;
  while ((status = finger.getImage()) != FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Place your finger");
    lcd.setCursor(0, 1);
    lcd.print("on the sensor.");
    delay(2000);
    Serial.println("Place your finger on the sensor.");
    delay(1000);
  }

  Serial.println("Fingerprint image taken.");
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("Failed to create template.");
    return;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Place same finger");
  lcd.setCursor(0, 1);
  lcd.print("on the sensor.");
  delay(2000);
  Serial.println("Place the same finger again.");
  while ((status = finger.getImage()) != FINGERPRINT_OK) {
    Serial.println("Waiting for second fingerprint image.");
    delay(1000);
  }

  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("Failed to create second template.");
    return;
  }

  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("Failed to create fingerprint model.");
    return;
  }

  if (finger.storeModel(teacherId) != FINGERPRINT_OK) {
    Serial.println("Failed to store fingerprint model.");
    return;
  }
   lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("finger print");
  lcd.setCursor(0, 1);
  lcd.print("enroll sucesses");
  delay(2000);
  Serial.println("Fingerprint enrollment successful.");
  saveTeacherData(teacherId, teacherName, coursecode);
}

// Save teacher data to the CSV file
void saveTeacherData(int teacherId, const String& teacherName, const String& coursecode) {
  // Save to local storage first
  File file = SPIFFS.open(teacherDataFile, FILE_APPEND);
  if (!file) {
    Serial.println("Failed to open teacher data file for appending.");
    return;
  }

  String dataLine = String(teacherId) + "," + teacherName + "," + coursecode + "\n";
  file.print(dataLine);
  file.close();

  // Create JSON for Supabase
  StaticJsonDocument<200> doc;
  doc["teacher_id"] = teacherId;
  doc["name"] = teacherName;
  doc["course_code"] = coursecode;
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  bool cloudSaved = sendToSupabase("teachers", jsonString);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Teacher saved:");
  lcd.setCursor(0, 1);
  lcd.print(teacherName);
  delay(2000);

  if (cloudSaved) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Cloud sync done");
    delay(1000);
  }

  Serial.println("Teacher data saved: " + dataLine);
}

// Serve the teacher data file for download
void handleDownloadTeachers() {
  if (!authenticateUser()) {
    return;
  }

  File file = SPIFFS.open(teacherDataFile, FILE_READ);
  if (!file) {
    server.send(500, "text/html", "<h2>Failed to open teacher data file.</h2>");
    return;
  }

  server.streamFile(file, "text/csv");
  file.close();
}

// Student enrollment page
void handleEnrollStudent() {
  if (!authenticateUser()) {
    return;
  }

  String htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
      <body>
        <h2>Student Enrollment</h2>
        <form action="/enrollStudentSubmit" method="POST">
          <label for="studentId">Student ID (SL):</label><br>
          <input type="text" id="studentId" name="studentId"><br><br>
          <label for="studentName">Student Name:</label><br>
          <input type="text" id="studentName" name="studentName"><br><br>
          <label for="stdroll">Student Roll Number:</label><br>
          <input type="text" id="stdroll" name="stdroll"><br><br>
          <input type="submit" value="Enroll Student">
        </form>
      </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}

// Enroll a student and take attendance
void enrollStudent(int studentId, const String& studentName, const String& stdroll) {
   if (!initializeFingerprint()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sensor Error!");
        lcd.setCursor(0, 1);
        lcd.print("Try Again Later");
        delay(3000);
        return;
    }
  Serial.print("Starting enrollment for Student ID: ");
  Serial.println(studentId);
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("StR ENR st id");
  lcd.setCursor(0, 1);
  lcd.print(stdroll);
  delay(2000);

  // Directly initialize the fingerprint sensor here
  mySerial.begin(57600);
  delay(1000);
  finger.begin(57600);

  int status;
  while ((status = finger.getImage()) != FINGERPRINT_OK) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("place :");
    lcd.setCursor(0, 1);
    lcd.print("finger");
    Serial.println("Place your finger on the sensor.");
    delay(1000);
  }

  Serial.println("Fingerprint image taken.");
  if (finger.image2Tz(1) != FINGERPRINT_OK) {
    Serial.println("Failed to create template.");
    return;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("place same:");
  lcd.setCursor(0, 1);
  lcd.print("finger again");
  delay(2000);
  Serial.println("Place the same finger again.");
  while ((status = finger.getImage()) != FINGERPRINT_OK) {
    Serial.println("Waiting for second fingerprint image.");
    delay(1000);
  }

  if (finger.image2Tz(2) != FINGERPRINT_OK) {
    Serial.println("Failed to create second template.");
    return;
  }

  if (finger.createModel() != FINGERPRINT_OK) {
    Serial.println("Failed to create fingerprint model.");
    return;
  }

  if (finger.storeModel(studentId) != FINGERPRINT_OK) {
    Serial.println("Failed to store fingerprint model.");
    return;
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Finferprint Enroll");
  lcd.setCursor(0, 1);
  lcd.print("success");
  delay(2000);
  Serial.println("Fingerprint enrollment successful.");
  saveStudentData(studentId, studentName, stdroll);
}

// Save student data to the CSV file
void saveStudentData(int studentId, const String& studentName, const String& stdroll) {
 // Save locally
 File file = SPIFFS.open(studentDataFile, FILE_APPEND);
 if (!file) {
   Serial.println("Failed to open student data file for appending.");
   return;
 }

 String dataLine = String(studentId) + "," + studentName + "," + stdroll + "\n";
 file.print(dataLine);
 file.close();

 // Save to Supabase
 StaticJsonDocument<200> doc;
 doc["student_id"] = studentId;
 doc["name"] = studentName;
 doc["roll_number"] = stdroll;
 
 String jsonString;
 serializeJson(doc, jsonString);
 bool cloudSaved = sendToSupabase("students", jsonString);

 Serial.println("Student data saved: " + dataLine);
 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("stnd saved:");
 lcd.setCursor(0, 1);
 lcd.print(stdroll);
 delay(2000);

 if (cloudSaved) {
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Cloud sync done"); 
   delay(1000);
 }
}
void handleDownloadStudents() {
  if (!authenticateUser()) {
    return;
  }

  File file = SPIFFS.open(studentDataFile, FILE_READ);
  if (!file) {
    server.send(500, "text/html", "<h2>Failed to open student data file.</h2>");
    return;
  }

  server.streamFile(file, "text/csv");
  file.close();
}


// for attensence mode

void handleattendancemode() {
  if (!authenticateUser()) {
    return;
  }

  String htmlPage = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <meta http-equiv="refresh" content="0;url=/attendanceMode">
        <title>Starting Attendance Mode</title>
    </head>
    <body>
        <h2>Starting attendance mode...</h2>
        <script>
            window.location.href = '/attendanceMode';
        </script>
    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", htmlPage);
}


bool isTeacherFingerprintValid(int teacherId, const String& coursecode) {
    if (!SPIFFS.exists(teacherDataFile)) {
        Serial.println("Teacher data file does not exist!");
        return false;
    }
    File file = SPIFFS.open(teacherDataFile, "r"); // Open the teacher CSV file
    if (!file) {
        Serial.println("Failed to open teacher data file!");
        return false;
    }

    while (file.available()) {
        
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue; // Skip empty lines

        // Parse CSV line
        int comma1 = line.indexOf(',');
        int comma2 = line.lastIndexOf(',');

        if (comma1 == -1 || comma2 == -1 || comma1 == comma2) continue;

        String idStr = line.substring(0, comma1);
        String courseCodeStr = line.substring(comma2 + 1);

        if (idStr.toInt() == teacherId && courseCodeStr.equalsIgnoreCase(coursecode)) {
            file.close();
            return true; // Valid teacher ID and course code found
        }
    }

    file.close();
    return false; // Teacher ID and course code do not match
}




int getFingerprintID() {
    uint8_t p = finger.getImage();
    if (p == FINGERPRINT_NOFINGER) {
        Serial.println("No finger detected. Waiting for input...");
        return -1;  // Return early if no finger is detected
    }

    if (p != FINGERPRINT_OK) {
        Serial.println("Error capturing fingerprint image.");
        return -1;  // Handle other capture errors
    }

    p = finger.image2Tz();
    if (p != FINGERPRINT_OK) {
        Serial.println("Error converting fingerprint image.");
        return -1;
    }

    p = finger.fingerFastSearch();
    if (p != FINGERPRINT_OK) {
        Serial.println("No matching fingerprint found.");
        return -1;
    }

    return finger.fingerID; // Return the ID of the matched fingerprint
}




void clearSensorBuffer() {
    while (finger.getImage() != FINGERPRINT_NOFINGER) {
        delay(100); // Wait until the sensor reports no finger
    }
}


bool verifyTeacherFingerprint(String coursecode) {
    int teacherId;
    clearSensorBuffer();  // Ensure the sensor buffer is clear before starting

    while (true) {
        teacherId = getFingerprintID();  // Fetch fingerprint ID
        if (teacherId == -1) {
            Serial.println("No fingerprint detected. Please place your finger.");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("No finger found");
            lcd.setCursor(0, 1);
            lcd.print("Try again...");
            delay(2000);
            continue;  // Keep waiting until a fingerprint is detected
        }

        // Check if the fingerprint matches the teacher ID and course code
        if (isTeacherFingerprintValid(teacherId, coursecode)) {
            Serial.println("Teacher fingerprint verified successfully.");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Verification");
            lcd.setCursor(0, 1);
            lcd.print("Success!");
            delay(2000);
            return true;  // Verification successful
        } else {
            Serial.println("Fingerprint detected but verification failed.");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Verification");
            lcd.setCursor(0, 1);
            lcd.print("Failed!");
            delay(2000);
            return false;  // Verification failed
        }
    }
}


int processStudentAttendance(const String &coursecode) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Scan Student Finger");
    Serial.println("Awaiting student fingerprint...");

    while (true) {
        int result = finger.getImage();
        if (result == FINGERPRINT_NOFINGER) {
            delay(100);
            continue;
        }

        if (result != FINGERPRINT_OK) {
            Serial.println("Error capturing fingerprint image.");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Error Reading");
            lcd.setCursor(0, 1);
            lcd.print("Try Again");
            delay(2000);
            return -2;
        }

        result = finger.image2Tz();
        if (result != FINGERPRINT_OK) {
            Serial.println("Error converting fingerprint image.");
            return -2;
        }

        result = finger.fingerFastSearch();
        if (result != FINGERPRINT_OK) {
            Serial.println("Fingerprint not recognized!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Invalid Finger");
            delay(2000);
            return -1;
        }

        int studentId = finger.fingerID;
        
        // First check if student exists in database
        String studentName = fetchStudentName(studentId);
        if (studentName.isEmpty() || studentName == "Unknown Student") {
            Serial.println("Fingerprint matched but student not in database!");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Unknown Student");
            lcd.setCursor(0, 1);
            lcd.print("Not Recorded");
            delay(2000);
            return -4; // New error code for unknown student
        }

        if (studentId < 0 || studentId >= MAX_STUDENTS) {
            Serial.println("Invalid student ID.");
            return -1;
        }

        if (attendedStudentsarray[studentId]) {
            Serial.println("Duplicate attendance detected.");
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Already Marked");
            lcd.setCursor(0, 1);
            lcd.print("Attendance");
            delay(2000);
            return -3;
        }

        attendedStudentsarray[studentId] = true;
        Serial.println("Fingerprint recognized. ID: " + String(studentId));
        return studentId;
    }
}





String fetchStudentName(int studentId) {
  File studentFile = SPIFFS.open("/students.csv", FILE_READ);
  if (!studentFile) {
    Serial.println("Failed to open student file!");
    return "";
  }

  while (studentFile.available()) {
    String line = studentFile.readStringUntil('\n');
    int idIndex = line.indexOf(',');
    int nameIndex = line.lastIndexOf(',');

    String id = line.substring(0, idIndex);
    String name = line.substring(idIndex + 1, nameIndex);

    if (id.toInt() == studentId) {
      studentFile.close();
      return name;
    }
  }

  studentFile.close();
  return "Unknown Student";
}


void attendanceMode() {
    if (!initializeFingerprint()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sensor Error!");
        lcd.setCursor(0, 1);
        lcd.print("Try Again Later");
        delay(3000);
        return;
    }
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Attendance Mode");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    Serial.println("Attendance mode started.");

    String coursecode = "";

    // Reset the attendance tracking array
    memset(attendedStudentsarray, false, sizeof(attendedStudentsarray));

    // Prompt for course code
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Course Code:");
    Serial.println("Awaiting course code...");

    while (true) {
        char key = keypad.getKey();
        if (key) {
            if (key == '#') {  // End of input
                Serial.println("Course code entered: " + coursecode);
                break;
            } else {
                coursecode += key;
                lcd.print(key);
                Serial.print(key);
            }
        }
    }

    // Verify teacher fingerprint
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Verify Teacher...");
    Serial.println("Place teacher's finger on the sensor.");

    if (!verifyTeacherFingerprint(coursecode)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Verification Failed");
        Serial.println("Teacher fingerprint verification failed.");
        delay(3000);
        return;
    }

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Attendance Start");
    Serial.println("Attendance started for course: " + coursecode);

    while (true) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Awaiting Student");

        int studentId = processStudentAttendance(coursecode);
           
        if (studentId > 0) {
            String studentName = fetchStudentName(studentId);
            // Double check to ensure student exists in database
            if (!studentName.isEmpty() && studentName != "Unknown Student") {
                saveAttendance(coursecode, studentName, studentId);
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Marked for:");
                lcd.setCursor(0, 1);
                lcd.print(studentName);
                Serial.println("Attendance recorded for: " + studentName);
                delay(2000);
            }
        } else if (studentId == -1) {
            Serial.println("Invalid fingerprint. Attendance not recorded.");
        } else if (studentId == -3) {
            Serial.println("Duplicate attendance. Skipping.");
        } else if (studentId == -4) {
            Serial.println("Unknown student. Attendance not recorded.");
        }

        // Allow teacher to end attendance session
        if (verifyTeacherFingerprint(coursecode)) {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Attendance Ended");
            Serial.println("Attendance session ended.");
            delay(3000);
            break;
        }
    }
}

void saveAttendance(String coursecode, String studentName, int studentId) {
 time_t now = time(nullptr);
 struct tm *localTime = localtime(&now);
 char timestamp[32];
 strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localTime);

 // Save locally
 File attendanceFile = SPIFFS.open("/attendance.csv", FILE_APPEND);
 if (!attendanceFile) {
   Serial.println("Failed to open attendance file!");
   return;
 }

 String record = coursecode + "," + studentName + "," + String(studentId) + "," + String(timestamp) + "\n";
 attendanceFile.print(record);
 attendanceFile.close();
 Serial.println("Attendance saved: " + record);

 // Save to Supabase
 StaticJsonDocument<300> doc;
 doc["course_code"] = coursecode;
 doc["student_name"] = studentName;
 doc["student_id"] = studentId;
 doc["timestamp"] = timestamp;
 
 String jsonString;
 serializeJson(doc, jsonString);
 bool cloudSaved = sendToSupabase("attendance", jsonString);

 lcd.clear();
 lcd.setCursor(0, 0);
 lcd.print("Marked for:");
 lcd.setCursor(0, 1);
 lcd.print(studentName);
 delay(2000);

 if (cloudSaved) {
   lcd.clear();
   lcd.setCursor(0, 0);
   lcd.print("Cloud sync done");
   delay(1000);
 }
}



void handleDownloadAttendance() {
    if (!authenticateUser()) {
        return;
    }

    String htmlPage = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Attendance View</title>
            <style>
                .container {
                    max-width: 800px;
                    margin: 0 auto;
                    padding: 20px;
                }
                .option-button {
                    display: inline-block;
                    padding: 10px 20px;
                    margin: 10px;
                    background-color: #4CAF50;
                    color: white;
                    text-decoration: none;
                    border-radius: 5px;
                }
                select {
                    padding: 8px;
                    margin: 10px 0;
                    width: 200px;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h2>Select Course Code</h2>
                <select id="courseCode">
)rawliteral";

    // Read course codes from teachers.csv
    File teacherFile = SPIFFS.open("/teachers.csv", "r");
    if (teacherFile) {
        std::set<String> uniqueCourses;  // Using set to store unique course codes
        
        while (teacherFile.available()) {
            String line = teacherFile.readStringUntil('\n');
            int lastComma = line.lastIndexOf(',');
            if (lastComma != -1) {
                String courseCode = line.substring(lastComma + 1);
                courseCode.trim();
                if (courseCode.length() > 0) {
                    uniqueCourses.insert(courseCode);
                }
            }
        }
        teacherFile.close();

        // Add options for each unique course code
        for (const String& course : uniqueCourses) {
            htmlPage += "                    <option value=\"" + course + "\">" + course + "</option>\n";
        }
    }

    htmlPage += R"rawliteral(
                </select>
                <br><br>
                <a href="#" onclick="viewTotalAttendance()" class="option-button">View Total Attendance</a>
                <a href="#" onclick="viewDatewiseAttendance()" class="option-button">View Date-wise Attendance</a>

                <div id="attendanceData"></div>

                <script>
                    function viewTotalAttendance() {
                        var courseCode = document.getElementById('courseCode').value;
                        window.location.href = '/attendanceDetails?courseCode=' + courseCode + '&type=total';
                    }

                    function viewDatewiseAttendance() {
                        var courseCode = document.getElementById('courseCode').value;
                        window.location.href = '/attendanceDetails?courseCode=' + courseCode + '&type=datewise';
                    }
                </script>
            </div>
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", htmlPage);
}

void handleViewAttendance() {
    if (!authenticateUser()) {
        return;
    }

    String htmlPage = R"rawliteral(
        <h2>Attendance View</h2>
        <form action="/attendanceDetails" method="GET">
            <label for="courseCode">Select Course Code:</label>
            <select name="courseCode" id="courseCode">
            <h3>Select an option:</h3>
                <a href="/attendanceDetails?courseCode=)" + courseCode + R"rawliteral(&viewType=total">Total Attendance</a><br>
                <a href="/attendanceDetails?courseCode=)" + courseCode + R"rawliteral(&viewType=datewise">Date-wise Attendance</a><br>
                <a href="/datewiseDownload">Download Date-wise Attendance</a>
    )rawliteral";

    // Fetch unique course codes from teachers.csv
    std::set<String> courseCodes;
    File teacherFile = SPIFFS.open("/teachers.csv", "r");
    if (teacherFile) {
        while (teacherFile.available()) {
            String line = teacherFile.readStringUntil('\n');
            int lastComma = line.lastIndexOf(',');
            if (lastComma != -1) {
                String courseCode = line.substring(lastComma + 1);
                courseCodes.insert(courseCode);
            }
        }
        teacherFile.close();
    }

    // Generate options for each unique course code
    for (const String& courseCode : courseCodes) {
        htmlPage += "<option value=\"" + courseCode + "\">" + courseCode + "</option>";
    }

    htmlPage += R"rawliteral(
            </select>
            <input type="submit" value="View Attendance">
        </form>
    )rawliteral";

    server.send(200, "text/html", htmlPage);
}


void handleAttendanceDetails1() {
    if (!authenticateUser()) {
        return;
    }

    String courseCode = server.arg("courseCode");
    String viewType = server.arg("viewType");

    std::map<String, int> attendanceData; // Ensure this is declared
    int totalClasses = 0; // Ensure this is declared

    String htmlPage = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Attendance Details - )rawliteral" + courseCode + R"rawliteral(</title>
            <style>
                table {
                    border-collapse: collapse;
                    width: 100%;
                }
                th, td {
                    border: 1px solid black;
                    padding: 8px;
                    text-align: left;
                }
            </style>
        </head>
        <body>
            <h2>Attendance Details for )rawliteral" + courseCode + R"rawliteral(</h2>
    )rawliteral";

    if (viewType == "total") {
        // Fetch attendance data from attendance.csv
        std::map<String, int> attendanceData;
        int totalClasses = 0;
        File attendanceFile = SPIFFS.open("/attendance.csv", "r");
        if (attendanceFile) {
            while (attendanceFile.available()) {
                String line = attendanceFile.readStringUntil('\n');
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);
                int thirdComma = line.indexOf(',', secondComma + 1);
                if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
                    String fileCourseCode = line.substring(0, firstComma);
                    String studentRoll = line.substring(secondComma + 1, thirdComma);
                    if (fileCourseCode == courseCode) {
                        attendanceData[studentRoll]++;
                        totalClasses++;
                    }
                }
            }
            attendanceFile.close();
        }

        // Generate HTML table for total attendance
        htmlPage += R"rawliteral(
            <table>
                <tr>
                    <th>Roll</th>
                    <th>Name</th>
                    <th>Attendance</th>
                    <th>Marks</th>
                </tr>
        )rawliteral";

        File studentFile = SPIFFS.open("/students.csv", "r");
        if (studentFile) {
            while (studentFile.available()) {
                String line = studentFile.readStringUntil('\n');
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);
                if (firstComma != -1 && secondComma != -1) {
                    String studentRoll = line.substring(0, firstComma);
                    String studentName = line.substring(firstComma + 1, secondComma);
                    int attendance = attendanceData[studentRoll];
                    double attendancePercentage = (attendance * 100.0) / totalClasses;
                    int marks = 0;
                    if (attendancePercentage >= 95) {
                        marks = 8;
                    } else if (attendancePercentage >= 90) {
                        marks = 7;
                    } else if (attendancePercentage >= 85) {
                        marks = 6;
                    } else if (attendancePercentage >= 80) {
                        marks = 5;
                    } else if (attendancePercentage >= 75) {
                        marks = 4;
                    } else if (attendancePercentage >= 70) {
                        marks = 3;
                    } else if (attendancePercentage >= 65) {
                        marks = 2;
                    } else if (attendancePercentage >= 60) {
                        marks = 1;
                    }
                    htmlPage += "<tr><td>" + studentRoll + "</td><td>" + studentName + "</td><td>" + String(attendance) + "/" + String(totalClasses) + " (" + String(attendancePercentage, 2) + "%)</td><td>" + String(marks) + "</td></tr>";
                }
            }
            studentFile.close();
        }

        htmlPage += R"rawliteral(
            </table>
        )rawliteral";
    } else if (viewType == "datewise") {
        // Fetch unique dates from attendance.csv for the given course code
        std::set<String> dates;
        File attendanceFile = SPIFFS.open("/attendance.csv", "r");
        if (attendanceFile) {
            while (attendanceFile.available()) {
                String line = attendanceFile.readStringUntil('\n');
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);
                int thirdComma = line.indexOf(',', secondComma + 1);
        
           if (firstComma != -1 && secondComma != -1 && thirdComma != -1) {
            String fileCourseCode = line.substring(0, firstComma);
            String studentRoll = line.substring(secondComma + 1, thirdComma);
            if (fileCourseCode == courseCode) {
                attendanceData[studentRoll]++;
                totalClasses++;
            }
        }
    }
    attendanceFile.close();
        }

        // Generate HTML form for selecting date
        htmlPage += R"rawliteral(
            <form action="/datewiseAttendance" method="GET">
                <input type="hidden" name="courseCode" value=")rawliteral" + courseCode + R"rawliteral(">
                <label for="date">Select Date:</label>
                <select name="date" id="date">
        )rawliteral";

        for (const String& date : dates) {
            htmlPage += "<option value=\"" + date + "\">" + date + "</option>";
        }

        htmlPage += R"rawliteral(
                </select>
                <br><br>
                <input type="submit" value="View Attendance">
            </form>
        )rawliteral";
    }

    htmlPage += R"rawliteral(
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", htmlPage);
}


void handleDatewiseAttendance() {
    if (!authenticateUser()) {
        return;
    }

    String courseCode = server.arg("courseCode");
    String date = server.arg("date");

    String htmlPage = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Date-wise Attendance - )rawliteral" + courseCode + R"rawliteral(</title>
            <style>
                table {
                    border-collapse: collapse;
                    width: 100%;
                }
                th, td {
                    border: 1px solid black;
                    padding: 8px;
                    text-align: left;
                }
            </style>
        </head>
        <body>
            <h2>Date-wise Attendance for )rawliteral" + courseCode + R"rawliteral( on )rawliteral" + date + R"rawliteral(</h2>
            <table>
                <tr>
                    <th>Roll</th>
                    <th>Name</th>
                    <th>Attendance</th>
                </tr>
    )rawliteral";

    // Fetch attendance data from attendance.csv for the given date and course code
    std::set<String> attendedStudents;
    File attendanceFile = SPIFFS.open("/attendance.csv", "r");
    if (attendanceFile) {
        while (attendanceFile.available()) {
            String line = attendanceFile.readStringUntil('\n');
            int firstComma = line.indexOf(',');
            int secondComma = line.indexOf(',', firstComma + 1);
            int thirdComma = line.indexOf(',', secondComma + 1);
            int lastComma = line.lastIndexOf(',');
            if (firstComma != -1 && secondComma != -1 && thirdComma != -1 && lastComma != -1) {
                String fileCourseCode = line.substring(0, firstComma);
                String studentRoll = line.substring(secondComma + 1, thirdComma);
                String dateTime = line.substring(lastComma + 1);
                String fileDate = dateTime.substring(0, 10);
                if (fileCourseCode == courseCode && fileDate == date) {
                    attendedStudents.insert(studentRoll);
                }
            }
        }
        attendanceFile.close();
    }

    // Generate HTML table for date-wise attendance
    File studentFile = SPIFFS.open("/students.csv", "r");
    if (studentFile) {
        while (studentFile.available()) {
            String line = studentFile.readStringUntil('\n');
            int firstComma = line.indexOf(',');
            int secondComma = line.indexOf(',', firstComma + 1);
            if (firstComma != -1 && secondComma != -1) {
                String studentRoll = line.substring(0, firstComma);
                String studentName = line.substring(firstComma + 1, secondComma);
                String attendance = (attendedStudents.count(studentRoll) > 0) ? "Present" : "Absent";
                htmlPage += "<tr><td>" + studentRoll + "</td><td>" + studentName + "</td><td>" + attendance + "</td></tr>";
            }
        }
        studentFile.close();
    }

    htmlPage += R"rawliteral(
            </table>
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", htmlPage);
}



// Function to delete all the CSV files
void deleteAllCSVFiles() {
  // Check and delete teachers.csv
  if (SPIFFS.exists(teacherDataFile)) {
    if (SPIFFS.remove(teacherDataFile)) {
      Serial.println("teachers.csv file deleted successfully.");
    } else {
      Serial.println("Failed to delete teachers.csv file.");
    }
  } else {
    Serial.println("teachers.csv file does not exist.");
  }

  // Check and delete students.csv
  if (SPIFFS.exists(studentDataFile)) {
    if (SPIFFS.remove(studentDataFile)) {
      Serial.println("students.csv file deleted successfully.");
    } else {
      Serial.println("Failed to delete students.csv file.");
    }
  } else {
    Serial.println("students.csv file does not exist.");
  }

  // Check and delete attendance.csv
  if (SPIFFS.exists("/attendance.csv")) {
    if (SPIFFS.remove("/attendance.csv")) {
      Serial.println("attendance.csv file deleted successfully.");
    } else {
      Serial.println("Failed to delete attendance.csv file.");
    }
  } else {
    Serial.println("attendance.csv file does not exist.");
  }
}

// Call this function through an HTTP endpoint or directly in the code
void handleDeleteAllCSVFiles() {
  if (!authenticateUser()) {
    return;
  }
  
  deleteAllCSVFiles();
  server.send(200, "text/html", "<h2>All CSV files deleted successfully!</h2>");
}
// Define a structure to store temporary attendance records
struct AttendanceRecord {
    int studentId;
    String studentName;
    bool isValid;
};

// Array to store temporary attendance records
const int MAX_TEMP_RECORDS = 100;
AttendanceRecord tempAttendance[MAX_TEMP_RECORDS];
int tempAttendanceCount = 0;

void startAttendanceProcess() {
    if (!initializeFingerprint()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sensor Error!");
        lcd.setCursor(0, 1);
        lcd.print("Try Again Later");
        delay(3000);
        isAttendanceMode = false;
        return;
    }
    
    // Reset temporary attendance storage
    tempAttendanceCount = 0;
    memset(attendedStudentsarray, false, sizeof(attendedStudentsarray));
    
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Attendance Mode");
    lcd.setCursor(0, 1);
    lcd.print("Starting...");
    Serial.println("Attendance mode started.");

    String coursecode = "";

    // Prompt for course code
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Course Code:");
    Serial.println("Awaiting course code...");

    while (isAttendanceMode) {
        char key = keypad.getKey();
        if (key) {
            if (key == 'B') {  // Cancel attendance mode
                isAttendanceMode = false;
                cancelAttendanceProcess();
                return;
            }
            else if (key == '*') {  // Backspace functionality
                if (coursecode.length() > 0) {
                    coursecode = coursecode.substring(0, coursecode.length() - 1);
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Enter Course Code:");
                    lcd.setCursor(0, 1);
                    lcd.print(coursecode);
                }
            }
            else if (key == '#') {  // End of input
                if (coursecode.length() > 0) {
                    Serial.println("Course code entered: " + coursecode);
                    break;
                }
            }
            else if (key != 'A') {  // Normal number input
                coursecode += key;
                lcd.setCursor(0, 1);
                lcd.print(coursecode);
                Serial.print(key);
            }
        }
    }

    if (!isAttendanceMode) return;

    // Verify initial teacher fingerprint
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Verify Teacher...");
    Serial.println("Place teacher's finger on the sensor.");

    if (!verifyTeacherFingerprint(coursecode)) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Verification Failed");
        Serial.println("Teacher fingerprint verification failed.");
        delay(3000);
        isAttendanceMode = false;
        return;
    }

    // Main attendance collection loop
    while (isAttendanceMode) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Place Finger");
        lcd.setCursor(0, 1);
        lcd.print("Or End Session");

        int studentId = getFingerprintID();
        
        // Check if it's the teacher's fingerprint
        if (studentId > 0 && isTeacherFingerprintValid(studentId, coursecode)) {
            // Teacher verified - end session and save attendance
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Saving Records...");
            
            // Save all collected attendance records
            for (int i = 0; i < tempAttendanceCount; i++) {
                if (tempAttendance[i].isValid) {
                    saveAttendance(coursecode, tempAttendance[i].studentName, tempAttendance[i].studentId);
                }
            }
            
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Session Ended");
            lcd.setCursor(0, 1);
            lcd.print("Records: ");
            lcd.print(tempAttendanceCount);
            delay(3000);
            isAttendanceMode = false;
            break;
        }
        
        // Process student fingerprint
        if (studentId > 0) {
            String studentName = fetchStudentName(studentId);
            if (!studentName.isEmpty() && studentName != "Unknown Student" && !attendedStudentsarray[studentId]) {
                // Valid student found and not already marked
                attendedStudentsarray[studentId] = true;
                
                // Store in temporary array
                if (tempAttendanceCount < MAX_TEMP_RECORDS) {
                    tempAttendance[tempAttendanceCount].studentId = studentId;
                    tempAttendance[tempAttendanceCount].studentName = studentName;
                    tempAttendance[tempAttendanceCount].isValid = true;
                    tempAttendanceCount++;
                    
                    lcd.clear();
                    lcd.setCursor(0, 0);
                    lcd.print("Recorded:");
                    lcd.setCursor(0, 1);
                    lcd.print(studentName);
                    delay(1500);
                }
            }else if (studentName.isEmpty() || studentName == "Unknown Student") {
                lcd.clear();
                lcd.setCursor(0, 0);
                lcd.print("Unknown Student");
                lcd.setCursor(0, 1);
                lcd.print("Not Recorded");
                delay(2000);

        }}

        // Check for cancel key
        char key = keypad.getKey();
        if (key == 'B') {
            cancelAttendanceProcess();
            break;
        }
        
        delay(100);  // Small delay to prevent too rapid scanning
    }
}

void cancelAttendanceProcess() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Session Cancelled");
    lcd.setCursor(0, 1);
    lcd.print("No Records Saved");
    Serial.println("Attendance mode cancelled.");
    delay(2000);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Ready");
    isAttendanceMode = false;
    tempAttendanceCount = 0;  // Reset temporary storage
}

void handleAttendanceDetails() {
    if (!authenticateUser()) {
        return;
    }

    String courseCode = server.arg("courseCode");
    String type = server.arg("type");

    String htmlPage = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <title>Attendance Details</title>
            <style>
                .container {
                    max-width: 900px;
                    margin: 0 auto;
                    padding: 20px;
                }
                table {
                    width: 100%;
                    border-collapse: collapse;
                    margin: 20px 0;
                }
                th, td {
                    border: 1px solid #ddd;
                    padding: 8px;
                    text-align: left;
                }
                th {
                    background-color: #4CAF50;
                    color: white;
                }
                .download-link {
                    display: inline-block;
                    padding: 5px 10px;
                    background-color: #4CAF50;
                    color: white;
                    text-decoration: none;
                    border-radius: 3px;
                    margin: 5px;
                }
            </style>
        </head>
        <body>
            <div class="container">
                <h2>Attendance Details for Course: )rawliteral" + courseCode + R"rawliteral(</h2>
    )rawliteral";

    if (type == "total") {
        // Calculate total attendance
        std::map<String, int> studentAttendance;
        int totalClasses = 0;
        std::set<String> uniqueDates;

        // Read attendance.csv to get total classes and attendance counts
        File attendanceFile = SPIFFS.open("/attendance.csv", "r");
        if (attendanceFile) {
            while (attendanceFile.available()) {
                String line = attendanceFile.readStringUntil('\n');
                if (line.startsWith(courseCode + ",")) {
                    int firstComma = line.indexOf(',');
                    int secondComma = line.indexOf(',', firstComma + 1);
                    int thirdComma = line.indexOf(',', secondComma + 1);
                    
                    String studentId = line.substring(secondComma + 1, thirdComma);
                    String date = line.substring(thirdComma + 1, thirdComma + 11); // Extract date part
                    
                    uniqueDates.insert(date);
                    studentAttendance[studentId]++;
                }
            }
            attendanceFile.close();
        }

        totalClasses = uniqueDates.size(); // Number of unique dates is total classes

        // Create attendance table
        htmlPage += R"rawliteral(
            <table>
                <tr>
                    <th>Roll Number</th>
                    <th>Name</th>
                    <th>Attendance</th>
                    <th>Percentage</th>
                    <th>Marks</th>
                </tr>
        )rawliteral";

        // Read students.csv and display attendance data
        File studentFile = SPIFFS.open("/students.csv", "r");
        if (studentFile) {
            while (studentFile.available()) {
                String line = studentFile.readStringUntil('\n');
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma + 1);
                
                if (firstComma != -1 && secondComma != -1) {
                    String studentId = line.substring(0, firstComma);
                    String studentName = line.substring(firstComma + 1, secondComma);
                    String rollNumber = line.substring(secondComma + 1);
                    
                    int attended = studentAttendance[studentId];
                    float percentage = totalClasses > 0 ? (attended * 100.0 / totalClasses) : 0;
                    int marks = 0;
                    
                    // Calculate marks based on attendance percentage
                    if (percentage >= 95) marks = 8;
                    else if (percentage >= 90) marks = 7;
                    else if (percentage >= 85) marks = 6;
                    else if (percentage >= 80) marks = 5;
                    else if (percentage >= 75) marks = 4;
                    else if (percentage >= 70) marks = 3;
                    else if (percentage >= 65) marks = 2;
                    else if (percentage >= 60) marks = 1;

                    htmlPage += "<tr><td>" + rollNumber + "</td><td>" + studentName + 
                               "</td><td>" + String(attended) + "/" + String(totalClasses) + 
                               "</td><td>" + String(percentage, 1) + "%" +
                               "</td><td>" + String(marks) + "</td></tr>";
                }
            }
            studentFile.close();
        }
        
        htmlPage += "</table>";
        
    } else if (type == "datewise") {
        // Show date-wise attendance with download links
        std::set<String> dates;
        
        // Get unique dates for this course
        File attendanceFile = SPIFFS.open("/attendance.csv", "r");
        if (attendanceFile) {
            while (attendanceFile.available()) {
                String line = attendanceFile.readStringUntil('\n');
                if (line.startsWith(courseCode + ",")) {
                    int lastComma = line.lastIndexOf(',');
                    String date = line.substring(lastComma + 1, lastComma + 11); // Extract date part
                    dates.insert(date);
                }
            }
            attendanceFile.close();
        }

        htmlPage += R"rawliteral(
            <table>
                <tr>
                    <th>Date</th>
                    <th>Action</th>
                </tr>
        )rawliteral";

        for (const String& date : dates) {
            htmlPage += "<tr><td>" + date + "</td><td>" +
                       "<a href='/downloadDateAttendance?courseCode=" + courseCode + "&date=" + date + 
                       "' class='download-link'>Download</a></td></tr>";
        }
        
        htmlPage += "</table>";
    }

    htmlPage += R"rawliteral(
            </div>
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", htmlPage);
}

String generateTotalAttendance(const String& courseCode) {
    std::map<String, int> attendanceData;
    int totalClasses = 0;

    // Parse attendance.csv
    File attendanceFile = SPIFFS.open("/attendance.csv", "r");
    if (attendanceFile) {
        while (attendanceFile.available()) {
            String line = attendanceFile.readStringUntil('\n');
            auto parsed = parseCSVLine(line, {0, 1}); // Parse course code and roll number
            if (parsed[0] == courseCode) {
                attendanceData[parsed[1]]++;
                totalClasses++;
            }
        }
        attendanceFile.close();
    }

    // Generate HTML table
    String table = R"rawliteral(
        <table>
            <tr>
                <th>Roll</th>
                <th>Name</th>
                <th>Attendance</th>
                <th>Marks</th>
            </tr>
    )rawliteral";

    File studentFile = SPIFFS.open("/students.csv", "r");
    if (studentFile) {
        while (studentFile.available()) {
            String line = studentFile.readStringUntil('\n');
            auto parsed = parseCSVLine(line, {0, 1}); // Parse roll and name
            String roll = parsed[0];
            String name = parsed[1];
            int attendance = attendanceData[roll];
            double percentage = (attendance * 100.0) / totalClasses;
            int marks = calculateMarks(percentage);

            table += "<tr><td>" + roll + "</td><td>" + name + "</td><td>" +
                     String(attendance) + "/" + String(totalClasses) + " (" +
                     String(percentage, 2) + "%)</td><td>" + String(marks) + "</td></tr>";
        }
        studentFile.close();
    }

    table += "</table>";
    return table;
}

String generateDatewiseAttendance(const String& courseCode) {
    std::set<String> dates;

    // Parse attendance.csv to fetch dates
    File attendanceFile = SPIFFS.open("/attendance.csv", "r");
    if (attendanceFile) {
        while (attendanceFile.available()) {
            String line = attendanceFile.readStringUntil('\n');
            auto parsed = parseCSVLine(line, {0, 3}); // Parse course code and date
            if (parsed[0] == courseCode) {
                dates.insert(parsed[1].substring(0, 10)); // Extract date
            }
        }
        attendanceFile.close();
    }

    // Generate HTML table
    String table = R"rawliteral(
        <form action="/datewiseAttendance" method="GET">
            <label for="date">Select Date:</label>
            <select name="date" id="date">
    )rawliteral";

    for (const auto& date : dates) {
        table += "<option value=\"" + date + "\">" + date + "</option>";
    }

    table += R"rawliteral(
            </select>
            <br><br>
            <input type="hidden" name="courseCode" value=")rawliteral" + courseCode + R"rawliteral(">
            <input type="submit" value="View Attendance">
        </form>
    )rawliteral";

    return table;
}

int calculateMarks(double percentage) {
    if (percentage >= 95) return 8;
    else if (percentage >= 90) return 7;
    else if (percentage >= 85) return 6;
    else if (percentage >= 80) return 5;
    else if (percentage >= 75) return 4;
    else if (percentage >= 70) return 3;
    else if (percentage >= 65) return 2;
    else if (percentage >= 60) return 1;
    return 0;
}


// Usage Example: auto parsed = parseCSVLine(line, {0, 1});





void handleDateWiseDownload() {
    if (!authenticateUser()) {
        return;
    }

    String htmlPage = R"rawliteral(
        <h2>Date-wise Attendance Download</h2>
        <table border="1">
            <tr>
                <th>Date</th>
                <th>Download Link</th>
            </tr>
    )rawliteral";

    // Assuming you have a way to list available dates from your attendance records
    std::set<String> availableDates; // Populate this with actual dates from your CSV

    File attendanceFile = SPIFFS.open("/attendance.csv", "r");
    if (attendanceFile) {
        while (attendanceFile.available()) {
            String line = attendanceFile.readStringUntil('\n');
            // Extract date from timestamp and populate availableDates set
            // Example: Assuming timestamp is in the format "YYYY-MM-DD HH:MM:SS"
            int commaIndex = line.indexOf(',');
            if (commaIndex != -1) {
                String timestamp = line.substring(commaIndex + 1); // Get timestamp part
                String datePart = timestamp.substring(0, 10); // Extract date part
                availableDates.insert(datePart);
            }
        }
        attendanceFile.close();
    }

    for (const auto& date : availableDates) {
        htmlPage += "<tr><td>" + date + "</td><td><a href=\"/downloadAttendance?date=" + date +
                    "\">Download</a></td></tr>";
    }

    htmlPage += R"rawliteral(
        </table>
    )rawliteral";

   server.send(200, "text/html", htmlPage);
}
void handleDownloadAttendanceByDate() {
   if (!authenticateUser()) {
       return;
   }

   String dateRequested = server.arg("date");
   File fileToDownload; // Logic to open specific date's file

   // Logic to find and open the correct CSV file based on the requested date

   server.streamFile(fileToDownload, "text/csv");
   fileToDownload.close();
}


void handleDownloadAttendance25() {
    if (!authenticateUser()) {
        return;
    }

    File file = SPIFFS.open("/attendance.csv", "r");
    if (!file) {
        server.send(500, "text/html", "Failed to open attendance file.");
        return;
    }

    server.streamFile(file, "text/csv");
    file.close();
}




void handleEditAttendance() {
    if (!authenticateUser()) {
        return;
    }

    String htmlPage = R"rawliteral(
        <!DOCTYPE html>
        <html>
        <head>
            <style>
                table {
                    width: 100%;
                    border-collapse: collapse;
                    margin-bottom: 20px;
                }
                th, td {
                    border: 1px solid #ddd;
                    padding: 8px;
                    text-align: left;
                }
                th {
                    background-color: #4CAF50;
                    color: white;
                }
                .edit-btn {
                    background-color: #4CAF50;
                    color: white;
                    padding: 5px 10px;
                    border: none;
                    cursor: pointer;
                }
                .delete-btn {
                    background-color: #f44336;
                    color: white;
                    padding: 5px 10px;
                    border: none;
                    cursor: pointer;
                }
            </style>
        </head>
        <body>
            <h1>Download Attendance</h1>
            <a href="/downloadAttendance25">Download All Attendance CSV</a>
            <h2>Edit Attendance</h2>
            <table>
                <tr>
                    <th>Course Code</th>
                    <th>Student Name</th>
                    <th>Student ID</th>
                    <th>Timestamp</th>
                    <th>Actions</th>
                </tr>
    )rawliteral";

    File file = SPIFFS.open(attendanceDataFile, "r");
    if (file) {
        while (file.available()) {
            String line = file.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
                // Parse CSV line
                int comma1 = line.indexOf(',');
                int comma2 = line.indexOf(',', comma1 + 1);
                int comma3 = line.indexOf(',', comma2 + 1);

                String courseCode = line.substring(0, comma1);
                String studentName = line.substring(comma1 + 1, comma2);
                String studentId = line.substring(comma2 + 1, comma3);
                String timestamp = line.substring(comma3 + 1);

                // Create edit and delete buttons
                htmlPage += "<tr><td>" + courseCode + "</td><td>" + studentName + "</td><td>" +
                            studentId + "</td><td>" + timestamp + "</td><td>" +
                            "<button class='edit-btn' onclick='editAttendance(\"" + courseCode + "\", \"" + studentName + "\", \"" + studentId + "\", \"" + timestamp + "\")'>Edit</button>" +
                            "<button class='delete-btn' onclick='deleteAttendance25(\"" + courseCode + "\", \"" + studentName + "\", \"" + studentId + "\", \"" + timestamp + "\")'>Delete</button>" +
                            "</td></tr>";
            }
        }
        file.close();
    }

    htmlPage += R"rawliteral(
            </table>
            <script>
                function editAttendance(courseCode, studentName, studentId, timestamp) {
                    let newCourseCode = prompt("Enter new Course Code:", courseCode);
                    let newStudentName = prompt("Enter new Student Name:", studentName);
                    let newStudentId = prompt("Enter new Student ID:", studentId);
                    let newTimestamp = prompt("Enter new Timestamp:", timestamp);
                    if (newCourseCode && newStudentName && newStudentId && newTimestamp) {
                        window.location.href = `/updateAttendance?courseCode=${courseCode}&studentName=${studentName}&studentId=${studentId}&timestamp=${timestamp}` +
                                               `&newCourseCode=${newCourseCode}&newStudentName=${newStudentName}&newStudentId=${newStudentId}&newTimestamp=${newTimestamp}`;
                    }
                }
                function deleteAttendance25(courseCode, studentName, studentId, timestamp) {
                    if (confirm("Are you sure you want to delete this attendance record?")) {
                        window.location.href = `/deleteAttendance25?courseCode=${courseCode}&studentName=${studentName}&studentId=${studentId}&timestamp=${timestamp}`;
                    }
                }
            </script>
        </body>
        </html>
    )rawliteral";

    server.send(200, "text/html", htmlPage);
}
void handleUpdateAttendance() {
    if (!authenticateUser()) {
        return;
    }

    // Retrieve parameters from the request
    String courseCode = server.arg("courseCode");
    String studentName = server.arg("studentName");
    String studentId = server.arg("studentId");
    String timestamp = server.arg("timestamp");

    // Retrieve new values from the request
    String newCourseCode = server.arg("newCourseCode");
    String newStudentName = server.arg("newStudentName");
    String newStudentId = server.arg("newStudentId");
    String newTimestamp = server.arg("newTimestamp");

    // Open the old file for reading and a temporary file for writing
    File oldFile = SPIFFS.open(attendanceDataFile, "r");
    File newFile = SPIFFS.open("/temp_attendance.csv", "w");

    if (!oldFile || !newFile) {
        server.send(500, "text/plain", "File operation failed");
        return;
    }

    while (oldFile.available()) {
        String line = oldFile.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            // Parse the current line
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);

            String existingCourseCode = line.substring(0, comma1);
            String existingStudentName = line.substring(comma1 + 1, comma2);
            String existingStudentId = line.substring(comma2 + 1, comma3);
            String existingTimestamp = line.substring(comma3 + 1);

            // Update the record if it matches the specified parameters
            if (existingCourseCode == courseCode &&
                existingStudentName == studentName &&
                existingStudentId == studentId &&
                existingTimestamp == timestamp) {
                newFile.println(newCourseCode + "," + newStudentName + "," + newStudentId + "," + newTimestamp);
            } else {
                newFile.println(line); // Keep the original record
            }
        }
    }

    oldFile.close();
    newFile.close();

    // Replace the old file with the updated file
    SPIFFS.remove(attendanceDataFile);
    SPIFFS.rename("/temp_attendance.csv", attendanceDataFile);

    // Redirect back to the edit page
    server.sendHeader("Location", "/editAttendance");
    server.send(303);
}
void handleDeleteAttendance25() {
    if (!authenticateUser()) {
        return;
    }

    // Retrieve parameters from the request
    String courseCode = server.arg("courseCode");
    String studentName = server.arg("studentName");
    String studentId = server.arg("studentId");
    String timestamp = server.arg("timestamp");

    // Open the old file for reading and a temporary file for writing
    File oldFile = SPIFFS.open(attendanceDataFile, "r");
    File newFile = SPIFFS.open("/temp_attendance.csv", "w");

    if (!oldFile || !newFile) {
        server.send(500, "text/plain", "File operation failed");
        return;
    }

    while (oldFile.available()) {
        String line = oldFile.readStringUntil('\n');
        line.trim();
        if (line.length() > 0) {
            // Parse the current line
            int comma1 = line.indexOf(',');
            int comma2 = line.indexOf(',', comma1 + 1);
            int comma3 = line.indexOf(',', comma2 + 1);

            String existingCourseCode = line.substring(0, comma1);
            String existingStudentName = line.substring(comma1 + 1, comma2);
            String existingStudentId = line.substring(comma2 + 1, comma3);
            String existingTimestamp = line.substring(comma3 + 1);

            // Only write the line back if it doesn't match the specified record
            if (!(existingCourseCode == courseCode &&
                  existingStudentName == studentName &&
                  existingStudentId == studentId &&
                  existingTimestamp == timestamp)) {
                newFile.println(line);
            }
        }
    }

    oldFile.close();
    newFile.close();

    // Replace the old file with the updated file
    SPIFFS.remove(attendanceDataFile);
    SPIFFS.rename("/temp_attendance.csv", attendanceDataFile);

    // Redirect back to the edit page
    server.sendHeader("Location", "/editAttendance");
    server.send(303);
}



void handleDownloadDateAttendance() {
    if (!authenticateUser()) {
        return;
    }

    String courseCode = server.arg("courseCode");
    String date = server.arg("date");

    // Create a temporary file for the filtered attendance
    File tempFile = SPIFFS.open("/temp_download.csv", "w");
    if (!tempFile) {
        server.send(500, "text/plain", "Error creating temporary file");
        return;
    }

    // Write CSV header
    tempFile.println("Course Code,Student Name,Roll Number,Time");

    // Read and filter attendance data
    File attendanceFile = SPIFFS.open("/attendance.csv", "r");
    if (attendanceFile) {
        while (attendanceFile.available()) {
            String line = attendanceFile.readStringUntil('\n');
            if (line.startsWith(courseCode + ",") && line.indexOf(date) != -1) {
                tempFile.println(line);
            }
        }
        attendanceFile.close();
    }

    tempFile.close();

    // Send the file
    File downloadFile = SPIFFS.open("/temp_download.csv", "r");
    if (downloadFile) {
        server.streamFile(downloadFile, "text/csv");
        downloadFile.close();
    }

    // Clean up
    SPIFFS.remove("/temp_download.csv");
}








void setup() {
  
  Serial.begin(115200);
  mySerial.begin(57600); // Start serial for the sensor
  finger.begin(57600);
   Wire.begin(21, 22);
    lcd.begin(16, 2);
    lcd.backlight();
    lcd.clear();
    lcd.print("Starting up...");
    
    // Initialize fingerprint sensor
    if (!initializeFingerprint()) {
        Serial.println("Failed to initialize fingerprint sensor!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Sensor Failed!");
        delay(2000);
    }  
  configTime(0, 0, "pool.ntp.org"); // Initialize the Adafruit Fingerprint library

  if (finger.verifyPassword()) {
        Serial.println("Fingerprint sensor detected!");
  } else {
        Serial.println("Fingerprint sensor not detected or communication error.");
        while (1) delay(1000); // Halt execution if the sensor is not detected
  }
  preferences.begin("WiFi", false);

  SPIFFS.begin();
  if (!SPIFFS.begin(true)) { // Format SPIFFS if mounting fails
        Serial.println("Failed to mount SPIFFS!");
        return;
    }
    Serial.println("SPIFFS mounted successfully.");

  
  

  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  lcd.setCursor(0, 0);    // Set cursor to first row, first column
  lcd.print("Connecting...");
  

  if (ssid == "" || password == "") {
    startAccessPoint();
  } else {
    // Try connecting to saved Wi-Fi network
    if (!connectToWiFi(ssid.c_str(), password.c_str())) {
      startAccessPoint();
    }
  }
  server.on("/editTeacher", HTTP_GET, handleEditTeacher);
  server.on("/updateTeacher", HTTP_GET, handleUpdateTeacher);
  server.on("/deleteTeacher", HTTP_GET, handleDeleteTeacher);
  server.on("/editStudent", HTTP_GET, handleEditStudent);
  server.on("/updateStudent", HTTP_GET, handleUpdateStudent);
  server.on("/deleteStudent", HTTP_GET, handleDeleteStudent);
  server.on("/editAttendance",  HTTP_GET, handleEditAttendance);
  server.on("/updateAttendance",  HTTP_GET,handleUpdateAttendance);
  server.on("/deleteAttendance25",  HTTP_GET,handleDeleteAttendance25);
  server.on("/", HTTP_GET, handleRoot);
  server.on("/wifi", HTTP_GET, handleWifiSettings);
  server.on("/saveWifi", HTTP_POST, handleSaveWifi);
  server.on("/enrollTeacher", HTTP_GET, handleEnrollTeacher);
  server.on("/enrollTeacherSubmit", HTTP_POST, []() {
    String teacherId = server.arg("teacherId");
    String teacherName = server.arg("teacherName");
    String coursecode = server.arg("coursecode");

    enrollTeacher(teacherId.toInt(), teacherName, coursecode);
    server.send(200, "text/html", "<h2>Teacher enrolled successfully!</h2>");
  });
  server.on("/downloadTeachers", HTTP_GET, handleDownloadTeachers);
  server.on("/enrollStudent", HTTP_GET, handleEnrollStudent);
  server.on("/enrollStudentSubmit", HTTP_POST, []() {
    String studentId = server.arg("studentId");
    String studentName = server.arg("studentName");
    String stdroll = server.arg("stdroll");

    enrollStudent(studentId.toInt(), studentName, stdroll);
    server.send(200, "text/html", "<h2>Student enrolled successfully!</h2>");
  });
  server.on("/downloadStudents", HTTP_GET, handleDownloadStudents);
  server.on("/attendancemode", HTTP_GET, handleattendancemode);
  server.on("/attendanceMode", HTTP_GET, attendanceMode);
  
  server.on("/deleteAllFiles", HTTP_GET, handleDeleteAllCSVFiles);
  
  server.on("/deleteAllFiles", HTTP_GET, handleDeleteAllCSVFiles);
  server.on("/downloadAttendance", HTTP_GET, handleDownloadAttendance);
  server.on("/viewAttendance", HTTP_GET, handleViewAttendance);
  server.on("/attendanceDetails", HTTP_GET, handleAttendanceDetails);
  server.on("/datewiseAttendance", HTTP_GET, handleDatewiseAttendance);
  server.on("/attendanceDetails", HTTP_GET, handleAttendanceDetails1);
  server.on("/datewiseDownload", handleDateWiseDownload);
  server.on("/downloadAttendance", handleDownloadAttendanceByDate);
  
  server.on("/deleteAttendanceRecord", handleDeleteAttendanceRecord);
  server.on("/downloadAttendance25",handleDownloadAttendance25);
  server.on("/downloadDateAttendance", HTTP_GET, handleDownloadDateAttendance);
 
  
  
  
  

  server.begin();
}


 // To hold the entered characters


  
 

void loop() {

  server.handleClient();
  char key = keypad.getKey();
   if (key) {
    switch (key) {
      case 'A':
        if (!isAttendanceMode) {
          isAttendanceMode = true;
          startAttendanceProcess();
        }
        break;
      case 'B':
        if (isAttendanceMode) {
          isAttendanceMode = false;
          cancelAttendanceProcess();
        }
        break;
    }
  }
 

}