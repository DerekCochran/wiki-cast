from langchain_core.messages import HumanMessage, AIMessage, ToolMessage
from typing import TypedDict, List
from langgraph.graph import StateGraph, START, END
from langchain_core.tools import tool
from langchain_core.messages import BaseMessage, HumanMessage
from langchain_openai import ChatOpenAI # Or your LLM of choice
from langchain_community.tools.file_management import FileSearchTool, ListDirectoryTool
from tools.test_build_tools import run_tests, compile_code
from tools.file_tools import view_file_lines, search_grep, edit_file_replace, list_directory


class AgentState(TypedDict):
    test_results: str
    compilation_errors: str
    messages: List[BaseMessage]
    is_passing: bool


def test_runner_node(state: AgentState):
    print("--- RUNNING TESTS ---")
    results = run_tests.invoke({})
    
    # Simple check to see if tests passed (modify based on your test runner's output)
    is_passing = "FAIL" not in results and "Error" not in results
    
    return {
        "test_results": results,
        "is_passing": is_passing
    }

def ai_fixer_node(state: AgentState):
    print("--- AI IS FIXING CODE ---")
    llm = ChatOpenAI(
        model="qwen2.5-coder",           # Must match the exact name from `ollama list`
        base_url="http://localhost:11434/v1",
        api_key="ollama",                # Required by the constructor, but ignored by Ollama
        temperature=0                    # Keep it 0 for deterministic coding/fixing
    )
    
    # Provide the AI with the context it needs
    prompt = f"The tests failed with the following output:\n{state['test_results']}\n Please fix the files."
    
    llm_with_tools = llm.bind_tools([view_file_lines, edit_file_replace, search_grep, list_directory, run_tests, compile_code])
    response = llm_with_tools.invoke(prompt)
    
    return {
        "messages": [response]
    }

# 4. Define the Routing Logic (Conditional Edge)
def decide_next_step(state: AgentState):
    if state["is_passing"]:
        return "complete"
    else:
        return "fix_code"

# 5. Build the Graph
workflow = StateGraph(AgentState)

# Add our nodes to the graph
workflow.add_node("run_tests", test_runner_node)
workflow.add_node("ai_fixer", ai_fixer_node)

# Set the entry point
workflow.add_edge(START, "run_tests")

# Add conditional routing after running tests
workflow.add_conditional_edges(
    "run_tests",
    decide_next_step,
    {
        "complete": END,
        "fix_code": "ai_fixer"
    }
)

# After the AI fixes the code, send it back to get tested again
workflow.add_edge("ai_fixer", "run_tests")

# Compile the graph into an executable runnable
app = workflow.compile()

# 6. Run the process
if __name__ == "__main__":
    initial_state = {
        "test_results": "",
        "compilation_errors": "",
        "messages": [],
        "is_passing": False
    }
    
    # This will run the loop until decide_next_step returns "complete" (END)
    final_output = app.invoke(initial_state)
    print("Process complete! Final status: Passing =", final_output["is_passing"])