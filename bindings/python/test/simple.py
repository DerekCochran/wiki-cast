from wiki_cast import WikiConfig, WikiParser
# Minimal valid config with required protocol field
config = WikiConfig.from_string('{"ext": ["ref"], "html": [[],[],[]], "protocol": "https?://|http://"}')
parser = WikiParser(config)
result = parser.parse_to_string("== Natively Compiled SIMD ==")
print("Parse result:", result)