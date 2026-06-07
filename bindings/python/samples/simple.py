from wiki_cast import WikiConfig, WikiParser
# Minimal valid config with required protocol field
config = WikiConfig.from_string('{"ext": ["ref"], "html": [[],[],[]], "protocol": "https?://|http://"}')
parser = WikiParser(config)

# Parse to string, This is only usefull as a round trip comparison
result_string = parser.parse_to_string("== Natively Compiled SIMD ==")
print("Parse to string result:", result_string)

# Parse to dict: Can be manipulated and traversed as a normal Python dict, but is not the raw AST object
result_dict = parser.parse_to_dict("== Natively Compiled SIMD ==")
print("Parse to dict result:", result_dict)

# Parse to token (raw AST object)
# DANGEROUS: The memory may be owned by the root token, and if it is freed, the child tokens will become invalid. 
# Only use if in a single function scope and you are sure the root token will not be freed until you are done with the child tokens.
result_token = parser.parse("== Natively Compiled SIMD ==")
print("Parse to token result:")
print(f"  Type: {result_token.type}")
print(f"  Subtype: {result_token.subtype}")
print(f"  Children: {len(result_token.children)}")
for child in result_token.children:
    if hasattr(child, 'type'):
        print(f"    Child type: {child.type}, level: {child.level}")
    else:
        print(f"    Text: {child}")