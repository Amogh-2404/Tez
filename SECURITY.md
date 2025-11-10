# Security Policy

## Supported Versions

We actively maintain and provide security updates for the following versions:

| Version | Supported          |
| ------- | ------------------ |
| 1.x.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Security Features

Tez implements multiple security layers to protect against common web server vulnerabilities:

### 1. Path Traversal Protection
- **Implementation**: `file_server.cpp:57-93`
- **Protection**: Sanitizes all file paths using `std::filesystem::weakly_canonical`
- **Validates**: Paths remain within the `static/` directory
- **Blocks**: `../`, `~`, absolute paths, drive letters (Windows)
- **Response**: 403 Forbidden for invalid paths

### 2. Request Size Limits
- **Max Content-Length**: 10 MB (configurable in `main.cpp:21`)
- **Max Header Size**: 8 KB (configurable in `main.cpp:22`)
- **Max Keep-Alive Requests**: 1000 per connection
- **Response**: 413 Payload Too Large or 431 Request Header Fields Too Large

### 3. Input Validation
- **Content-Length**: Validated before memory allocation
- **Headers**: Size checked before parsing
- **Body**: Only read if Content-Length is valid and within limits
- **Response**: 400 Bad Request for malformed input

### 4. LRU Cache Security
- **Implementation**: `middleware.cpp:10-82`
- **Eviction**: Least-recently-used (not clear-all)
- **TTL**: 60-second expiration on all cached entries
- **Thread-safe**: Mutex protection for concurrent access
- **Prevents**: Cache poisoning and memory exhaustion attacks

### 5. Thread Safety
- **Mutexes**: All shared state protected
- **Caches**: Thread-safe LRU implementation
- **Config**: Loaded once with mutex protection
- **Logging**: Thread-safe file writes

## Reporting a Vulnerability

We take security seriously. If you discover a security vulnerability, please follow these steps:

### DO

1. **Email** security details to: ramogh2404@gmail.com
2. **Include**:
   - Description of the vulnerability
   - Steps to reproduce
   - Potential impact
   - Suggested fix (if available)
   - Your contact information
3. **Wait** for our response before public disclosure
4. **Encrypt** sensitive information using our PGP key (if available)

### DON'T

- ❌ Publicly disclose the vulnerability before we've addressed it
- ❌ Test on production systems without permission
- ❌ Exploit vulnerabilities for malicious purposes
- ❌ Demand payment or compensation

## Response Timeline

| Action | Timeline |
|--------|----------|
| **Initial Response** | Within 48 hours |
| **Vulnerability Confirmation** | Within 7 days |
| **Fix Development** | Within 30 days (severity dependent) |
| **Patch Release** | Within 45 days |
| **Public Disclosure** | After patch release |

## Severity Classification

We use the following severity levels:

### Critical (CVSS 9.0-10.0)
- Remote code execution
- Authentication bypass
- Data exfiltration

**Response**: Immediate action, hotfix within 7 days

### High (CVSS 7.0-8.9)
- Privilege escalation
- SQL injection
- Path traversal exploitation

**Response**: Fix within 14 days

### Medium (CVSS 4.0-6.9)
- Information disclosure
- Denial of service
- Session hijacking

**Response**: Fix within 30 days

### Low (CVSS 0.1-3.9)
- Minor information leak
- Configuration issues

**Response**: Fix in next release

## Known Limitations

### Current Security Gaps (Acknowledged)

1. **No TLS/SSL Support**
   - Status: Planned for v1.1 (Phase 2)
   - Workaround: Use reverse proxy (nginx, Caddy) for HTTPS
   - Impact: Traffic is unencrypted

2. **No Rate Limiting**
   - Status: Planned for v1.2 (Phase 3)
   - Impact: Vulnerable to request flooding
   - Workaround: Use firewall rules or reverse proxy

3. **No Authentication/Authorization**
   - Status: Application-level responsibility
   - Impact: No built-in access control
   - Workaround: Implement in route handlers

4. **Basic Logging**
   - Status: Enhanced logging planned for v1.1
   - Impact: Limited intrusion detection
   - Workaround: Use external log aggregation

5. **No Request Timeout on Slow Clients**
   - Status: Partially mitigated by keep-alive limits
   - Impact: Vulnerable to slowloris attacks
   - Workaround: Use reverse proxy with timeout

## Security Best Practices

When deploying Tez, follow these recommendations:

### Production Deployment

1. **Use a Reverse Proxy**
   ```nginx
   # nginx configuration
   server {
       listen 443 ssl;
       ssl_certificate /path/to/cert.pem;
       ssl_certificate_key /path/to/key.pem;

       location / {
           proxy_pass http://localhost:8080;
           proxy_set_header Host $host;
           proxy_set_header X-Real-IP $remote_addr;
       }
   }
   ```

2. **Run as Non-Root User**
   ```bash
   # Create dedicated user
   sudo useradd -r -s /bin/false tez

   # Set permissions
   sudo chown -R tez:tez /opt/tez

   # Run with systemd (user=tez)
   ```

3. **Restrict File Permissions**
   ```bash
   # Binary
   chmod 755 /opt/tez/build/Tez

   # Config (readable only by tez user)
   chmod 600 /opt/tez/config.json

   # Static files (read-only)
   chmod 644 /opt/tez/static/*
   ```

4. **Enable Firewall**
   ```bash
   # UFW (Ubuntu)
   sudo ufw allow from 127.0.0.1 to any port 8080
   sudo ufw deny 8080
   ```

5. **Monitor Logs**
   ```bash
   # Watch for suspicious activity
   tail -f server.log | grep -E "(403|413|431|500)"
   ```

6. **Update Regularly**
   ```bash
   # Check for updates
   git pull origin main
   cd build && make
   sudo systemctl restart tez
   ```

### Security Checklist

Before deploying to production:

- [ ] Tez runs behind a reverse proxy with TLS
- [ ] Server runs as non-root user
- [ ] Firewall blocks direct access to port 8080
- [ ] Static directory contains only public files
- [ ] Config file is not world-readable
- [ ] Logs are monitored and rotated
- [ ] Dependency versions are up-to-date
- [ ] Security limits are configured appropriately
- [ ] Backup and disaster recovery plan exists

## Vulnerability History

### Version 1.0.0 (2025-01-09)

**Fixed Vulnerabilities:**

1. **Path Traversal (Critical)**
   - **CVE**: N/A (fixed before public release)
   - **Description**: Lack of path sanitization allowed reading arbitrary files
   - **Fix**: Implemented `sanitize_path()` with filesystem canonical check
   - **Commit**: See initial release

2. **Memory Exhaustion (High)**
   - **CVE**: N/A (fixed before public release)
   - **Description**: No Content-Length limit allowed memory exhaustion
   - **Fix**: Added 10 MB limit with 413 response
   - **Commit**: See initial release

3. **Cache Poisoning (Medium)**
   - **CVE**: N/A (fixed before public release)
   - **Description**: Clear-all eviction allowed cache flooding attacks
   - **Fix**: Implemented LRU eviction algorithm
   - **Commit**: See initial release

4. **MIME Type Performance (Low)**
   - **CVE**: N/A (fixed before public release)
   - **Description**: Linear MIME search with duplicates
   - **Fix**: Hash map lookup with O(1) complexity
   - **Commit**: See initial release

## Security Acknowledgments

We appreciate responsible disclosure and will publicly acknowledge researchers who help improve Tez's security:

- **Hall of Fame**: (Contributors will be listed here)

## Contact

- **Security Email**: ramogh2404@gmail.com
- **GitHub Security Advisories**: https://github.com/Amogh-2404/tez/security/advisories
- **PGP Key**: (Add if available)

## Additional Resources

- [OWASP Top 10](https://owasp.org/www-project-top-ten/)
- [CWE Top 25](https://cwe.mitre.org/top25/)
- [HTTP Security Headers](https://owasp.org/www-project-secure-headers/)
- [Boost.Asio Security](https://www.boost.org/doc/libs/release/doc/html/boost_asio.html)

---

**Last Updated**: 2025-01-09
**Version**: 1.0.0
