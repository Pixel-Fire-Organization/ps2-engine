using System;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Collections.Generic;
using Json.Schema;

namespace EcsGenerator
{
    // --- JSON DATA MODELS ---
    public class EcsSchema
    {
        [JsonPropertyName("components")]
        public List<EcsComponent> Components { get; set; } = new();

        [JsonPropertyName("entities")]
        public List<EcsEntity> Entities { get; set; } = new();
    }

    public class EcsComponent
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("description")]
        public string Description { get; set; } = "";

        [JsonPropertyName("properties")]
        public List<EcsProperty> Properties { get; set; } = new();

        [JsonPropertyName("actions")]
        public List<EcsAction> Actions { get; set; } = new();
    }

    public class EcsProperty
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("type")]
        public string Type { get; set; } = "";

        [JsonPropertyName("description")]
        public string Description { get; set; } = "";

        [JsonPropertyName("default")]
        public JsonElement Default { get; set; }

        [JsonPropertyName("options")]
        public Dictionary<string, string>? Options { get; set; }
    }

    public class EcsAction
    {
        [JsonPropertyName("name")]
        public string Name { get; set; } = "";

        [JsonPropertyName("engineHook")]
        public string EngineHook { get; set; } = "";
    }

    public class EcsEntity
    {
        [JsonPropertyName("classname")]
        public string Classname { get; set; } = "";

        [JsonPropertyName("classType")]
        public string ClassType { get; set; } = "PointClass";

        [JsonPropertyName("description")]
        public string Description { get; set; } = "";

        [JsonPropertyName("components")]
        public List<string> Components { get; set; } = new();
    }

    // --- MAIN PROGRAM ---
    class Program
    {
        static void Main(string[] args)
        {
            if (args.Length < 2)
            {
                Console.WriteLine("Usage: EcsGenerator <path_to_schema.json> <path_to_data.json> [output_header.h] [output.fgd]");
                Environment.Exit(1);
            }

            string schemaPath = Path.GetFullPath(args[0]);
            string dataPath = Path.GetFullPath(args[1]);
            
            string cppOutPath = args.Length > 2 ? Path.GetFullPath(args[2]) : Path.GetFullPath("GeneratedComponents.h");
            string fgdOutPath = args.Length > 3 ? Path.GetFullPath(args[3]) : Path.GetFullPath("engine.fgd");

            if (!File.Exists(schemaPath))
            {
                Console.WriteLine($"Error: Schema file not found at '{schemaPath}'");
                Environment.Exit(1);
            }
            if (!File.Exists(dataPath))
            {
                Console.WriteLine($"Error: Data file not found at '{dataPath}'");
                Environment.Exit(1);
            }

            // 1. Strict JSON Validation (JsonSchema.Net)
            var schema = JsonSchema.FromFile(schemaPath);
            string jsonText = File.ReadAllText(dataPath);
            
            using var document = JsonDocument.Parse(jsonText);

            var evaluateOptions = new EvaluationOptions
            {
                OutputFormat = OutputFormat.Hierarchical
            };

            var results = schema.Evaluate(document.RootElement, evaluateOptions);

            if (!results.IsValid)
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.WriteLine($"BUILD FAILED: ECS Data Validation Errors in '{dataPath}':");
                PrintErrorDetails(results);
                Console.ResetColor();
                Environment.Exit(1); 
            }

            Console.WriteLine("JSON Validation Passed!");

            // 2. Deserialization
            var options = new JsonSerializerOptions { ReadCommentHandling = JsonCommentHandling.Skip };
            EcsSchema data = JsonSerializer.Deserialize<EcsSchema>(jsonText, options)!;

            // 3. File Generation
            GenerateCppHeader(data, cppOutPath);
            GenerateFgd(data, fgdOutPath);

            Console.ForegroundColor = ConsoleColor.Green;
            Console.WriteLine($"Successfully generated:");
            Console.WriteLine($" -> {cppOutPath}");
            Console.WriteLine($" -> {fgdOutPath}");
            Console.ResetColor();
        }

        static void PrintErrorDetails(EvaluationResults results)
        {
            if (results.Errors != null)
            {
                foreach (var error in results.Errors)
                {
                    Console.WriteLine($"- {results.InstanceLocation}: {error.Value}");
                }
            }
            
            if (results.Details != null)
            {
                foreach (var detail in results.Details)
                {
                    PrintErrorDetails(detail);
                }
            }
        }

        // --- C++ GENERATOR ---
        static void GenerateCppHeader(EcsSchema data, string outPath)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath))!);

            using var writer = new StreamWriter(outPath);
            
            writer.WriteLine("// AUTO-GENERATED FILE - DO NOT EDIT MANUALLY");
            writer.WriteLine("#pragma once");
            writer.WriteLine("#include <cstdint>");
            writer.WriteLine("#include \"ECSHooks.h\"");
            writer.WriteLine();

            writer.WriteLine("enum class ComponentType : uint16_t {");
            writer.WriteLine("    None = 0,");
            for (int i = 0; i < data.Components.Count; i++)
            {
                string comma = (i == data.Components.Count - 1) ? "" : ",";
                writer.WriteLine($"    {data.Components[i].Name} = {i + 1}{comma}");
            }
            writer.WriteLine("};");
            writer.WriteLine();

            writer.WriteLine("struct IComponent {");
            writer.WriteLine("    ComponentType type = ComponentType::None;");
            writer.WriteLine("};");
            writer.WriteLine();

            foreach (var comp in data.Components)
            {
                writer.WriteLine($"struct {comp.Name} : public IComponent {{");
                
                foreach (var prop in comp.Properties)
                {
                    string cppType = MapToCppType(prop.Type);
                    string defaultVal = GetCppDefaultValue(prop.Default, cppType);
                    writer.WriteLine($"    {cppType} {prop.Name} = {defaultVal};");
                }

                if (comp.Properties.Count > 0) writer.WriteLine();

                writer.WriteLine($"    {comp.Name}() {{");
                writer.WriteLine($"        type = ComponentType::{comp.Name};");
                writer.WriteLine("    }");

                foreach (var action in comp.Actions)
                {
                    writer.WriteLine();
                    writer.WriteLine($"    void {action.Name}() {{");
                    writer.WriteLine($"        Hooks::{action.EngineHook}(this);");
                    writer.WriteLine("    }");
                }

                writer.WriteLine("};");
                writer.WriteLine();
            }
        }

        // --- FGD GENERATOR ---
        static void GenerateFgd(EcsSchema data, string outPath)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(outPath))!);

            using var writer = new StreamWriter(outPath);
            
            writer.WriteLine("// AUTO-GENERATED BY ECS TOOL");
            writer.WriteLine();

            foreach (var comp in data.Components)
            {
                // FIX 1: Detect if this component contains a 'studio' property and apply the new header binding
                string classModifiers = "";
                var studioProp = comp.Properties.Find(p => p.Type == "studio");
                if (studioProp != null)
                {
                    classModifiers = $"model({{ \"path\": {studioProp.Name} }}) ";
                }

                writer.WriteLine($"@BaseClass {classModifiers}= {comp.Name} [");
                foreach (var prop in comp.Properties)
                {
                    string fgdType = MapToFgdType(prop.Type);
                    string defaultVal = GetFgdDefaultValue(prop.Default, prop.Type);
                    
                    if (prop.Type == "choices" || prop.Type == "flags")
                    {
                        string assignOp = prop.Type == "choices" ? $" : {defaultVal} =" : " =";
                        writer.WriteLine($"    {prop.Name}({fgdType}){assignOp} [");
                        
                        if (prop.Options != null)
                        {
                            foreach (var option in prop.Options)
                            {
                                if (prop.Type == "flags") {
                                    bool isSet = (int.Parse(defaultVal) & int.Parse(option.Key)) != 0;
                                    writer.WriteLine($"        {option.Key} : \"{option.Value}\" : {(isSet ? 1 : 0)}");
                                } else {
                                    writer.WriteLine($"        {option.Key} : \"{option.Value}\"");
                                }
                            }
                        }
                        writer.WriteLine("    ]");
                    }
                    else
                    {
                        writer.WriteLine($"    {prop.Name}({fgdType}) : \"{prop.Description}\" : {defaultVal}");
                    }
                }
                writer.WriteLine("]");
                writer.WriteLine();
            }

            foreach (var ent in data.Entities)
            {
                string bases = string.Join(", ", ent.Components);
                string baseAttr = string.IsNullOrEmpty(bases) ? "" : $"base({bases}) ";
                string classType = string.IsNullOrEmpty(ent.ClassType) ? "PointClass" : ent.ClassType;
                
                // FIX 3: Add a default bounding box to PointClasses
                string sizeAttr = classType == "PointClass" ? "size(-16 -16 -16, 16 16 16) " : "";
                
                writer.WriteLine($"@{classType} {baseAttr}{sizeAttr}= {ent.Classname} : \"{ent.Description}\" []");
                writer.WriteLine();
            }
        }

        // --- TYPE MAPPING HELPERS ---
        static string MapToCppType(string jsonType)
        {
            return jsonType switch {
                "int" => "int",
                "float" => "float",
                "bool" => "bool",
                "string" => "const char*",
                "color255" => "const char*",
                "studio" => "const char*",
                "choices" => "int",
                "flags" => "uint32_t",
                _ => "int"
            };
        }

        static string MapToFgdType(string jsonType)
        {
            return jsonType switch {
                "int" => "integer",
                "float" => "string",
                "bool" => "choices",
                "string" => "string",
                "color255" => "color255",
                "studio" => "string", // FIX 2: Output as standard string in the property block
                "choices" => "choices",
                "flags" => "flags",
                _ => "string"
            };
        }

        static string GetCppDefaultValue(JsonElement element, string cppType)
        {
            if (element.ValueKind == JsonValueKind.Undefined || element.ValueKind == JsonValueKind.Null)
            {
                return cppType == "const char*" ? "\"\"" : "0";
            }

            string val = element.ToString() ?? "";
            
            if (cppType == "float" && !val.Contains(".")) val += ".0f";
            else if (cppType == "float") val += "f";
            else if (cppType == "bool") val = val.ToLower();
            else if (cppType == "const char*") val = $"\"{val}\"";

            return val;
        }

        static string GetFgdDefaultValue(JsonElement element, string originalType)
        {
            if (element.ValueKind == JsonValueKind.Undefined || element.ValueKind == JsonValueKind.Null)
            {
                return (originalType == "string" || originalType == "studio") ? "\"\"" : "0";
            }

            string val = element.ToString() ?? "";

            if (originalType == "bool") return val.ToLower() == "true" ? "1" : "0";
            if (originalType == "string" || originalType == "studio") return $"\"{val}\"";
            
            return val;
        }
    }
}