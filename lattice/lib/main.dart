// main.dart
import 'dart:async';
import 'dart:convert';
// 'dart:io' removed because not needed in the UI code
import 'package:flutter/foundation.dart' show kIsWeb;
import 'package:flutter/material.dart';
import 'package:file_picker/file_picker.dart';
import 'package:http/http.dart' as http;
import 'package:path/path.dart' as p;

void main() => runApp(const MyApp());

class MyApp extends StatelessWidget {
  const MyApp({super.key});
  @override
  Widget build(BuildContext context) {
    // Use a near-black background and white text for high-contrast dark theme
    final base = ThemeData.dark();
    return MaterialApp(
      title: 'Uploader',
      theme: base.copyWith(
        scaffoldBackgroundColor: Colors.black,
        primaryColor: Colors.black,
        appBarTheme: const AppBarTheme(
          backgroundColor: Colors.black,
          foregroundColor: Colors.white,
        ),
        textTheme: base.textTheme.apply(
          bodyColor: Colors.white,
          displayColor: Colors.white,
        ),
      ),
      home: const FileUploadPage(),
    );
  }
}

class FileUploadPage extends StatefulWidget {
  const FileUploadPage({super.key});
  @override
  State<FileUploadPage> createState() => _FileUploadPageState();
}

class _FileUploadPageState extends State<FileUploadPage> {
  PlatformFile? _picked;
  String _status = 'idle';
  Map<String, dynamic>? _responseJson;

  final String serverUrl = 'http://127.0.0.1:8080/upload';
  final String workersUrl = 'http://127.0.0.1:8080/workers';

  Future<void> pickFile() async {
    try {
      final res = await FilePicker.platform.pickFiles(
        withData: true, // This ensures bytes are loaded
        type: FileType.custom,
        allowedExtensions: [
          'cpp',
          'c',
          'h',
          'hpp',
          'cc',
          'cxx',
        ], // C++ file types
      );
      if (res == null || res.files.isEmpty) return;

      final file = res.files.first;

      // On web, ensure bytes are available
      if (kIsWeb && file.bytes == null) {
        setState(() {
          _status = 'error: Could not read file bytes on web';
          _responseJson = {'error': 'File bytes are null. Please try again.'};
        });
        return;
      }

      setState(() {
        _picked = file;
        _responseJson = null;
        _status = 'ready';
      });
    } catch (e) {
      setState(() {
        _status = 'error';
        _responseJson = {'error': e.toString()};
      });
    }
  }

  Future<void> upload() async {
    if (_picked == null) return;
    setState(() {
      _status = 'uploading';
      _responseJson = null;
    });

    try {
      final uri = Uri.parse(serverUrl);
      final request = http.MultipartRequest('POST', uri);

      // Use bytes if available (web or mobile with bytes)
      if (kIsWeb || (_picked!.bytes != null)) {
        if (_picked!.bytes == null) {
          throw Exception('File bytes are null. Cannot upload.');
        }
        final bytes = _picked!.bytes!;
        final filename = _picked!.name.isNotEmpty
            ? _picked!.name
            : 'uploaded_file.cpp';
        request.files.add(
          http.MultipartFile.fromBytes('file', bytes, filename: filename),
        );
      } else {
        // Desktop/mobile with file path
        if (_picked!.path == null) {
          throw Exception('File path is null. Cannot upload.');
        }
        final path = _picked!.path!;
        final filename = p.basename(path);
        request.files.add(
          await http.MultipartFile.fromPath('file', path, filename: filename),
        );
      }

      final streamed = await request.send();
      final resp = await http.Response.fromStream(streamed);

      setState(() {
        if (resp.statusCode >= 200 && resp.statusCode < 300) {
          try {
            _responseJson = json.decode(resp.body) as Map<String, dynamic>;
            _status = 'done';
          } catch (_) {
            _responseJson = {'text': resp.body};
            _status = 'done (non-json)';
          }
        } else {
          _status = 'failed ${resp.statusCode}';
          _responseJson = {'status': resp.statusCode, 'body': resp.body};
        }
      });
    } catch (e) {
      setState(() {
        _status = 'error';
        _responseJson = {'error': e.toString()};
      });
    }
  }

  Widget _buildFileInfo() {
    if (_picked == null) return const Text('No file chosen');
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text('Name: ${_picked!.name}'),
        Text('Size: ${_picked!.size} bytes'),
        if (_picked!.extension != null) Text('Ext: ${_picked!.extension}'),
        if (!kIsWeb && _picked!.path != null) Text('Path: ${_picked!.path}'),
        if (kIsWeb) Text('Platform: Web (using file bytes)'),
        if (_picked!.bytes != null)
          Text('Bytes loaded: ${_picked!.bytes!.length} bytes'),
      ],
    );
  }

  Widget _buildResponseView() {
    if (_responseJson == null) return const SizedBox.shrink();
    return Container(
      margin: const EdgeInsets.only(top: 12),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: Colors.black,
        border: Border.all(width: 1, color: Colors.white24),
        borderRadius: BorderRadius.circular(6),
      ),
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: Text(
          const JsonEncoder.withIndent('  ').convert(_responseJson),
          style: const TextStyle(color: Colors.white),
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        title: const Text('File uploader'),
        actions: [
          IconButton(
            tooltip: 'Workers status',
            icon: const Icon(Icons.storage),
            onPressed: () {
              Navigator.of(context).push(
                MaterialPageRoute(
                  builder: (_) => WorkersStatusPage(workersUrl: workersUrl),
                ),
              );
            },
          ),
        ],
      ),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            ElevatedButton.icon(
              onPressed: pickFile,
              icon: const Icon(Icons.attach_file),
              label: const Text('Pick file'),
            ),
            const SizedBox(height: 12),
            _buildFileInfo(),
            const SizedBox(height: 16),
            Row(
              children: [
                ElevatedButton(
                  onPressed: (_picked != null && _status != 'uploading')
                      ? upload
                      : null,
                  child: const Text('Upload'),
                ),
                const SizedBox(width: 12),
                Text(
                  'Status: $_status',
                  style: const TextStyle(color: Colors.white),
                ),
              ],
            ),
            _buildResponseView(),
            const Spacer(),
          ],
        ),
      ),
    );
  }
}

// Workers status page - polls /workers and displays a live table
class WorkersStatusPage extends StatefulWidget {
  final String workersUrl;
  const WorkersStatusPage({super.key, required this.workersUrl});

  @override
  State<WorkersStatusPage> createState() => _WorkersStatusPageState();
}

class _WorkersStatusPageState extends State<WorkersStatusPage> {
  Timer? _poller;
  List<Map<String, String>> _workers = [];
  String _lastError = '';
  DateTime? _lastUpdated;
  int _total = 0;
  int _available = 0;

  @override
  void initState() {
    super.initState();
    // fetch immediately, then poll every 2 seconds
    _fetchWorkers();
    _poller = Timer.periodic(
      const Duration(seconds: 2),
      (_) => _fetchWorkers(),
    );
  }

  Future<void> _fetchWorkers() async {
    try {
      final resp = await http.get(Uri.parse(widget.workersUrl));
      if (resp.statusCode >= 200 && resp.statusCode < 300) {
        final jsonBody = json.decode(resp.body) as Map<String, dynamic>;
        final List<dynamic> arr = jsonBody['workers'] ?? [];
        final List<Map<String, String>> parsed = arr.map((e) {
          final m = e as Map<String, dynamic>;
          return {
            'address': m['address'] as String? ?? '',
            'status': m['status'] as String? ?? '',
          };
        }).toList();

        setState(() {
          _workers = parsed;
          _lastError = '';
          _lastUpdated = DateTime.now();
          _total = (jsonBody['total'] ?? parsed.length) as int;
          _available = (jsonBody['available'] ?? 0) as int;
        });
      } else {
        setState(() {
          _lastError = 'HTTP ${resp.statusCode}';
        });
      }
    } catch (e) {
      setState(() {
        _lastError = e.toString();
      });
    }
  }

  @override
  void dispose() {
    _poller?.cancel();
    super.dispose();
  }

  Future<void> _refresh() async {
    await _fetchWorkers();
  }

  Widget _statusDot(String status) {
    final color = (status.toLowerCase() == 'available')
        ? Colors.green
        : Colors.red;
    return Row(
      children: [
        Container(
          width: 10,
          height: 10,
          decoration: BoxDecoration(color: color, shape: BoxShape.circle),
        ),
        const SizedBox(width: 8),
        Text(status, style: const TextStyle(color: Colors.white)),
      ],
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Workers status')),
      body: RefreshIndicator(
        color: Colors.white,
        onRefresh: _refresh,
        child: Padding(
          padding: const EdgeInsets.all(12),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Row(
                children: [
                  Text(
                    'Total: $_total',
                    style: const TextStyle(color: Colors.white),
                  ),
                  const SizedBox(width: 12),
                  Text(
                    'Available: $_available',
                    style: const TextStyle(color: Colors.white),
                  ),
                ],
              ),
              const SizedBox(height: 8),
              Text(
                _lastUpdated != null
                    ? 'Last update: ${_lastUpdated!.toLocal()}'
                    : 'Never updated',
                style: const TextStyle(color: Colors.white70),
              ),
              if (_lastError.isNotEmpty) ...[
                const SizedBox(height: 8),
                Text(
                  'Error: $_lastError',
                  style: const TextStyle(color: Colors.red),
                ),
              ],
              const SizedBox(height: 12),
              Expanded(
                child: _workers.isEmpty
                    ? Center(
                        child: _lastError.isEmpty
                            ? const CircularProgressIndicator(
                                valueColor: AlwaysStoppedAnimation<Color>(
                                  Colors.white,
                                ),
                              )
                            : Text(
                                'No workers',
                                style: const TextStyle(color: Colors.white),
                              ),
                      )
                    : SingleChildScrollView(
                        child: Container(
                          color: Colors.black,
                          child: DataTable(
                            headingTextStyle: const TextStyle(
                              color: Colors.white,
                              fontWeight: FontWeight.bold,
                            ),
                            dataTextStyle: const TextStyle(color: Colors.white),
                            columns: const [
                              DataColumn(label: Text('Address')),
                              DataColumn(label: Text('Status')),
                            ],
                            rows: _workers.map((w) {
                              return DataRow(
                                cells: [
                                  DataCell(
                                    Text(
                                      w['address'] ?? '',
                                      style: const TextStyle(
                                        color: Colors.white,
                                      ),
                                    ),
                                  ),
                                  DataCell(_statusDot(w['status'] ?? '')),
                                ],
                              );
                            }).toList(),
                          ),
                        ),
                      ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}