// main.dart
import 'dart:convert';
import 'dart:io' show File, Platform;
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
    return MaterialApp(title: 'Uploader', home: const FileUploadPage());
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

  Future<void> pickFile() async {
    final res = await FilePicker.platform.pickFiles(withData: true);
    if (res == null || res.files.isEmpty) return;
    setState(() {
      _picked = res.files.first;
      _responseJson = null;
      _status = 'ready';
    });
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

      if (kIsWeb || (_picked!.bytes != null)) {
        final bytes = _picked!.bytes!;
        final filename = p.basename(_picked!.name);
        request.files.add(
          http.MultipartFile.fromBytes('file', bytes, filename: filename),
        );
      } else {
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
        if (_picked!.size != null) Text('Size: ${_picked!.size} bytes'),
        if (_picked!.extension != null) Text('Ext: ${_picked!.extension}'),
        if (_picked!.path != null) Text('Path: ${_picked!.path}'),
      ],
    );
  }

  Widget _buildResponseView() {
    if (_responseJson == null) return const SizedBox.shrink();
    return Container(
      margin: const EdgeInsets.only(top: 12),
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        border: Border.all(width: 1),
        borderRadius: BorderRadius.circular(6),
      ),
      child: SingleChildScrollView(
        scrollDirection: Axis.horizontal,
        child: Text(const JsonEncoder.withIndent('  ').convert(_responseJson)),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('File uploader')),
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
                Text('Status: $_status'),
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
