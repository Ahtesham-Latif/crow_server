# Online Doctor Appointment Booking System

A secure, high-performance web-based application designed to streamline the process of scheduling medical appointments. This system allows patients to browse doctors by specialty, view real-time availability, and book or cancel appointments with ease.

**Author:** [Ahtesham Latif](https://github.com/Ahtesham-Latif)

---

## 🏥 Key Features

- **Doctor Directory**: Browse a categorized list of doctors by medical specialty (e.g., Cardiologist, Dentist)
- **Real-time Scheduling**: View available time slots and book appointments instantly with built-in double-booking protection
- **MVC Architecture**: Modular code structure with dedicated controllers for categories, doctors, schedules, and appointments
- **Secure Communication**: Full HTTPS/SSL support for encrypted data transmission
- **Automatic Notifications**: Instant email confirmations and reminders for all scheduled or cancelled appointments

---

## 🛠️ Technical Stack

| Component | Technology |
|-----------|-----------|
| **Backend Framework** | Crow (C++17) |
| **Database** | SQLite3 |
| **Security** | OpenSSL (TLS/SSL) |
| **Frontend** | HTML5 with Mustache templates |
| **Build System** | CMake 3.15+ |
| **Platform** | Cross-platform (Windows, Linux, macOS) |

---

## 📂 Project Structure

```
crow_backend/
├── config/              # Configuration files
├── controllers/         # Business logic layer (MVC)
├── models/             # Data structures and schemas
├── services/           # Utility services
├── routes/             # API route definitions
├── public/             # Frontend UI (HTML/CSS)
├── db/                 # Database files
├── Crow/               # Crow framework library
├── httplib/            # HTTP client library
├── CMakeLists.txt      # Build configuration
└── main.cpp            # Application entry point
```

### Directory Details

- **controllers/**: Contains business logic for managing appointments, doctors, schedules, categories, cancellations, and patients
- **models/**: Defines data structures for all entities (appointment, doctor, patient, schedule, etc.)
- **public/**: Static UI pages including category.html, doctor.html, appointment.html, confirmation.html, and cancellation.html
- **services/**: Utility functions and helpers (utils.h)
- **db/**: SQLite database files for persistent storage

---

## 🚀 Installation & Setup

### Prerequisites

- **Compiler**: C++17 compliant compiler (GCC 7+, Clang 5+, or MSVC 2017+)
- **CMake**: Version 3.15 or higher
- **Libraries**:
  - SQLite3
  - OpenSSL
  - Windows: ws2_32, mswsock (included in Win32 SDK)

### Build Instructions

1. **Clone the Repository**:
   ```bash
   git clone https://github.com/Ahtesham-Latif/crow_server.git
   cd crow_server
   ```

2. **Configure SSL Certificates**:
   Ensure `cert.pem` and `key.pem` are present in your configured directory for HTTPS support.

3. **Build with CMake**:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

4. **Run the Server**:
   ```bash
   ./server
   ```

   The server will start at `https://localhost:8443`

---

## 📋 System Architecture

The project follows an **MVC (Model-View-Controller)** pattern:

- **Models**: Data structures and database schemas
- **Controllers**: Business logic and request handling
- **Views**: Frontend HTML templates and static files
- **Services**: Shared utilities and helper functions

### API Endpoints

- **Categories**: View medical specialties
- **Doctors**: Browse doctors by specialty
- **Schedules**: Check doctor availability
- **Appointments**: Book, view, and manage appointments
- **Cancellations**: Cancel existing appointments
- **Patients**: Patient information management

---

## 🔒 Security Features

- **HTTPS/TLS Encryption**: All communication is encrypted using OpenSSL
- **Double-booking Prevention**: Automatic conflict detection
- **Email Notifications**: Secure appointment confirmations and reminders
- **Data Validation**: Input validation on all endpoints

---

## 📝 License

This project is open source. See the LICENSE file for details.

---

## 👨‍💼 Author

**Ahtesham Latif** - Full Stack Developer

- GitHub: [@Ahtesham-Latif](https://github.com/Ahtesham-Latif)

---

## 📧 Support

For issues, feature requests, or contributions, please visit the [GitHub Repository](https://github.com/Ahtesham-Latif/crow_server).

---

*Last Updated: February 2026*