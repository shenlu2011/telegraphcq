// HttpUpperTokenList.java
// $Id: HttpUpperTokenList.java,v 1.2 2003/07/17 05:19:12 sailesh Exp $
// (c) COPYRIGHT MIT and INRIA, 1996.
// Please first read the full copyright statement in file COPYRIGHT.html

package org.w3c.www.http;

/**
 * Parse a comma separated list of tokens.
 */

public class HttpUpperTokenList extends HttpTokenList {
    /**
     * Create a parsed token list, for emitting.
     */

    protected HttpUpperTokenList(String tokens[]) {
	super(tokens);
	this.casemode = CASE_UPPER;
    }

    /**
     * Create a token list from a comma separated list of tokens.
     */

    protected HttpUpperTokenList(String tokens) {
	super(tokens);
	this.casemode = CASE_UPPER;
    }

    /**
     * Create an empty token list for parsing.
     */

    protected HttpUpperTokenList() {
	super();
	this.casemode = CASE_UPPER;
    }

}
