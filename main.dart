import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:mesh_gradient/mesh_gradient.dart';
import 'package:animated_text_kit/animated_text_kit.dart';
import 'package:liquid_glass_widgets/liquid_glass_widgets.dart';
import 'package:file_picker/file_picker.dart';

final TextEditingController rip_valChange = TextEditingController();
final TextEditingController rbp_valChange = TextEditingController();
final TextEditingController textContainerfile = TextEditingController();
final TextEditingController dbgLog = TextEditingController();
final ValueNotifier<List<String>> dbgLogLines = ValueNotifier<List<String>>([]);
final TextEditingController AiLog = TextEditingController();
final ValueNotifier<List<String>> aiLogLines = ValueNotifier<List<String>>([]);

String filePath = '';
Process? dbggerrProcess;
void appendDbgLog(String text) {
  final now = DateTime.now();

  final time =
      '${now.hour.toString().padLeft(2, '0')}:'
      '${now.minute.toString().padLeft(2, '0')}:'
      '${now.second.toString().padLeft(2, '0')}';
  if (text.contains("RIP_VAL") || text.contains("RBP_VAL")){
    String rip;
    String rbp;

    if (text.contains("RIP_VAL")){
      RegExp exp1_rip = RegExp(r"0x.*?_");
      Match? match = exp1_rip.firstMatch(text);
      if (match != null){
        rip_valChange.text = match.group(0)!;
      }
    }
    if (text.contains("RBP_VAL")){
      RegExp exp1_rbp = RegExp(r"0x.*?_");
      Match? match = exp1_rbp.firstMatch(text);
      if (match != null){
        rbp_valChange.text = match.group(0)!;
      }
    }
  }
  else{
    dbgLogLines.value = [
    ...dbgLogLines.value,
    '[$time] $text',
  ];
  }
  
}

Future<void> startDebugger() async
{
  appendDbgLog("Starting Debugger..");
  dbggerrProcess = await Process.start('expltr_dbg.exe', []);

  
  appendDbgLog("Debugger started.");

  dbggerrProcess!.stdout
    .transform(utf8.decoder)
    .transform(const LineSplitter())
    .listen((line) {
  appendDbgLog(line);
});
  dbggerrProcess!.stderr.transform(utf8.decoder).transform(const LineSplitter()).listen((line){
    appendDbgLog("ERROR -> $line");
  });

  dbggerrProcess!.stdin.writeln(filePath);
}

Future<void> filePicker() async {
  FilePickerResult? result = await FilePicker.pickFiles();

  if (result != null) {
    String? path = result.files.single.path;

  if (path != null) {
    filePath = path;
    textContainerfile.text = filePath;
  }
  }
} 
void main() async{
  WidgetsFlutterBinding.ensureInitialized();
  await LiquidGlassWidgets.initialize();
  
  runApp(LiquidGlassWidgets.wrap(
      child: const MyApp(),
    ),);

}

class MyApp extends StatefulWidget {

  const MyApp({super.key});

  @override
  State<MyApp> createState() => _MyAppState();

}

class _MyAppState extends State<MyApp> {
  bool showIntro = true;

  @override
  void initState() {
    super.initState();

    //fade after 3 secs
    Future.delayed(const Duration(seconds: 3), () {
      setState(() {
        showIntro = false;
      });
    });
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      home: Scaffold(
        body: LayoutBuilder(
          builder: (context, constraints) {
            final double screenWidth = constraints.maxWidth;
            final double screenHeight = constraints.maxHeight;

            // responsive Werte, statt hart codierter Pixelzahlen
            final double logoSize = (screenWidth * 0.08).clamp(60.0, 140.0);
            final double logoPadding = (screenWidth * 0.03).clamp(16.0, 50.0);
            final double titleFontSize = (screenWidth * 0.045).clamp(26.0, 50.0);
            final double binaryFieldWidth = (screenWidth * 0.35).clamp(220.0, 600.0);
            final double regFieldWidth = (screenWidth * 0.18).clamp(180.0, 340.0);
            final double logPanelWidth = (screenWidth * 0.4).clamp(320.0, 750.0);
            final double logPanelHeight = (screenHeight * 0.55).clamp(320.0, 500.0);

            // gemeinsame Breite für RIP/RBP + Log-Panels, damit beide dieselbe linke Kante teilen
            final double contentWidth = (logPanelWidth * 2 + 24).clamp(300.0, screenWidth - 40.0);

            return Stack(
              children: [


                Positioned.fill(
                  child: AnimatedMeshGradient(
                    colors: [
                      Colors.black,
                      Colors.white,
                      Colors.red,
                      Colors.black,
                    ],
                    options: AnimatedMeshGradientOptions(),
                  ),
                ),

                //MORGEN ALS ERSTES HIER LOGO EINFüGEN, DANN ALS ZWEITES SPEICHER LESEN WAS IN RIP UND RBP STEHT (expltr_dbg.exe)
                Align(
                  alignment: Alignment.topRight, child: Padding(padding: EdgeInsets.only(right: logoPadding), child: 
                  Container(
                    width: logoSize,
                    height: logoSize,
                    child: Image.asset("assets/images/logo.png" ,fit: BoxFit.cover,),
                  ),)
                ),
                AnimatedOpacity(
                  opacity: showIntro ? 1.0 : 0.0,
                  duration: const Duration(milliseconds: 800),
                  child: Center(
                    child: DefaultTextStyle(
                      style: TextStyle(
                        fontSize: titleFontSize,
                        fontFamily: 'JetBrainsMono',
                        color: Colors.black,
                      ),
                      child: AnimatedTextKit(
                        animatedTexts: [
                          TypewriterAnimatedText(
                            'expl0itra_1.0',
                            speed: const Duration(milliseconds: 200),
                          ),
                        ],
                        totalRepeatCount: 1,
                      ),
                    ),
                  ),
                ),

Align(
  alignment: Alignment.bottomCenter,
  child: Padding(
    padding: EdgeInsets.only(
      bottom: MediaQuery.of(context).size.height * 0.03,
      left: 20,
      right: 20,
    ),
    child: Wrap(
      alignment: WrapAlignment.center,
      crossAxisAlignment: WrapCrossAlignment.center,
      spacing: 12,
      runSpacing: 12,
      children: [
        GlassIconButton(
          icon: Icon(Icons.upload),
          size: 48,
          shape: GlassIconButtonShape.roundedSquare,
          borderRadius: 12,
          onPressed: filePicker,
        ),

        SizedBox(
          width: binaryFieldWidth,
          child: GlassTextField(
            glowColor: Colors.blueGrey,
            controller: textContainerfile,
            placeholder: 'Binary-Path',
            obscureText: false,
            textStyle: TextStyle(
              fontFamily: 'JetBrainsMono',
              color: Colors.black,
            ),
            
          ),
        ),

        GlassButton.custom(
        
        height: 50,
        width: 150,
        onTap:() => startDebugger(),
        shape: const LiquidRoundedRectangle(
          borderRadius: 20,
        ),
        child: const Text(
          'START DEBUGGER',
          style: TextStyle(
            fontSize: 13,
            fontWeight: FontWeight.bold,
            fontFamily: 'JetBrainsMono',
          ),
        ),
      ),

      ],
    ),
  ),
),
//ende von Binary_path

// RIP/RBP + Log-Panels: EIN gemeinsamer, zentrierter Block mit fester Breite,
// dadurch teilen sich beide dieselbe linke Kante statt unabhängig voneinander zu hängen
Align(
  alignment: Alignment.center,
  child: SizedBox(
    width: contentWidth,
    child: Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      mainAxisSize: MainAxisSize.min,
      children: [
        Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            SizedBox(
              width: 50,
              child: GlassButton.custom(
                onTap: () {},
                height: 50,
                child: const Text(
                  'RIP',
                  style: TextStyle(
                    fontSize: 13,
                    fontWeight: FontWeight.bold,
                    fontFamily: 'JetBrainsMono',
                  ),
                ),
              ),
            ),

            const SizedBox(width: 15),

            SizedBox(
              width: regFieldWidth,
              child: GlassTextField(
                glowColor: Colors.blueGrey,
                controller: rip_valChange,
                placeholder: 'RIP-Value shown here..',
                obscureText: false,
                readOnly: true,
                textStyle: const TextStyle(
                  fontFamily: 'JetBrainsMono',
                  color: Colors.black,
                ),
              ),
            ),
          ],
        ),

        const SizedBox(height: 15),

        Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            SizedBox(
              width: 50,
              child: GlassButton.custom(
                onTap: () {},
                height: 50,
                
                child: const Text(
                  'RBP',
                  style: TextStyle(
                    fontSize: 13,
                    fontWeight: FontWeight.bold,
                    fontFamily: 'JetBrainsMono',
                  ),
                ),
              ),
            ),

            const SizedBox(width: 15),

            SizedBox(
              width: regFieldWidth,
              child: GlassTextField(
                glowColor: Colors.blueGrey,
                readOnly: true,
                controller: rbp_valChange,
                placeholder: 'RBP-Value shown here..',
                obscureText: false,
                textStyle: const TextStyle(
                  fontFamily: 'JetBrainsMono',
                  color: Colors.black,
                ),
              ),
            ),
          ],
        ),

        const SizedBox(height: 24),

        Wrap(
          spacing: 24,
          runSpacing: 24,
          children: [
          GlassContainer(
          width: logPanelWidth,
          height: logPanelHeight,
          child: Padding(
            padding: const EdgeInsets.all(16),
            child: ValueListenableBuilder<List<String>>(
        valueListenable: dbgLogLines,
        builder: (context, lines, child) {
          return ListView.builder(
            itemCount: lines.length,
            itemBuilder: (context, index) {
              return TweenAnimationBuilder<double>(
                key: ValueKey(index),
                tween: Tween(begin: 0.0, end: 1.0),
                duration: const Duration(milliseconds: 400),
                builder: (context, opacity, child) {
                  return Opacity(
                    opacity: opacity,
                    child: Padding(
                      padding: const EdgeInsets.symmetric(vertical: 2),
                      child: Text(
                    lines[index],
                    style: const TextStyle(
                      fontFamily: 'JetBrainsMono',
                      fontSize: 15,
                      color: Colors.black,
                    ),
                  )
                    ),
                  );
                },
              );
            },
          );
        },
      ),
          ),
        ),

            


          ],
        ),
      ],
    ),
  ),
),




              ],
            );
          },
        ),
      ),
    );
  }
}