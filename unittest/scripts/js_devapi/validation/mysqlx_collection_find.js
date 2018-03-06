//@ CollectionFind: valid operations after find
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after fields
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after groupBy
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after having
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after sort
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after limit
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after skip
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after lockShared
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after lockExclusive
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after bind
|All expected functions are available|
|No additional functions are available|

//@ CollectionFind: valid operations after execute
|All expected functions are available|
|No additional functions are available|

//@ Reusing CRUD with binding
|adam|
|alma|


//@# CollectionFind: Error conditions on find
||CollectionFind.find: Argument #1 is expected to be a string
||CollectionFind.find: Unterminated quoted string starting at position 8

//@# CollectionFind: Error conditions on fields
||Invalid number of arguments in CollectionFind.fields, expected at least 1 but got 0
||CollectionFind.fields: Argument #1 is expected to be a string, array of strings or a JSON expression
||CollectionFind.fields: Field selection criteria can not be empty
||CollectionFind.fields: Element #2 is expected to be a string
||CollectionFind.fields: Argument #1 is expected to be a JSON expression
||CollectionFind.fields: Argument #2 is expected to be a string

//@# CollectionFind: Error conditions on groupBy
||Invalid number of arguments in CollectionFind.groupBy, expected at least 1 but got 0
||CollectionFind.groupBy: Argument #1 is expected to be a string or an array of strings
||CollectionFind.groupBy: Grouping criteria can not be empty
||CollectionFind.groupBy: Element #2 is expected to be a string
||CollectionFind.groupBy: Argument #2 is expected to be a string

//@# CollectionFind: Error conditions on having
||Invalid number of arguments in CollectionFind.having, expected 1 but got 0
||CollectionFind.having: Argument #1 is expected to be a string

//@# CollectionFind: Error conditions on sort
||Invalid number of arguments in CollectionFind.sort, expected at least 1 but got 0
||CollectionFind.sort: Argument #1 is expected to be a string or an array of strings
||CollectionFind.sort: Sort criteria can not be empty
||CollectionFind.sort: Element #2 is expected to be a string
||CollectionFind.sort: Argument #2 is expected to be a string

//@# CollectionFind: Error conditions on limit
||Invalid number of arguments in CollectionFind.limit, expected 1 but got 0
||CollectionFind.limit: Argument #1 is expected to be an unsigned int

//@# CollectionFind: Error conditions on skip
||Invalid number of arguments in CollectionFind.skip, expected 1 but got 0
||CollectionFind.skip: Argument #1 is expected to be an unsigned int

//@# CollectionFind: Error conditions on lockShared
||Invalid number of arguments in CollectionFind.lockShared, expected 0 to 1 but got 2
||CollectionFind.lockShared: Argument #1 is expected to be one of DEFAULT, NOWAIT or SKIP_LOCKED

//@# CollectionFind: Error conditions on lockExclusive
||Invalid number of arguments in CollectionFind.lockExclusive, expected 0 to 1 but got 2
||CollectionFind.lockExclusive: Argument #1 is expected to be one of DEFAULT, NOWAIT or SKIP_LOCKED

//@# CollectionFind: Error conditions on bind
||Invalid number of arguments in CollectionFind.bind, expected 2 but got 0
||CollectionFind.bind: Argument #1 is expected to be a string
||CollectionFind.bind: Unable to bind value for unexisting placeholder: another

//@# CollectionFind: Error conditions on execute
||CollectionFind.execute: Missing value bindings for the following placeholders: data, years
||CollectionFind.execute: Missing value bindings for the following placeholders: data



//@ Collection.Find All
|All: 7|

//@ Collection.Find Filtering
|Males: 4|
|Females: 3|
|13 Years: 1|
|14 Years: 3|
|Under 17: 6|
|Names With A: 3|

//@ Collection.Find Field Selection
|1-Metadata Length: 2|
|1-Metadata Field: age|
|1-Metadata Field: name|
|2-Metadata Length: 1|
|2-Metadata Field: age|

//@ Collection.Find Sorting
|Find Asc 0 : adam|
|Find Asc 1 : alma|
|Find Asc 2 : angel|
|Find Asc 3 : brian|
|Find Asc 4 : carol|
|Find Asc 5 : donna|
|Find Asc 6 : jack|
|Find Desc 0 : jack|
|Find Desc 1 : donna|
|Find Desc 2 : carol|
|Find Desc 3 : brian|
|Find Desc 4 : angel|
|Find Desc 5 : alma|
|Find Desc 6 : adam|

//@ Collection.Find Limit and Offset
|Limit-Skip 0 : 4|
|Limit-Skip 1 : 4|
|Limit-Skip 2 : 4|
|Limit-Skip 3 : 4|
|Limit-Skip 4 : 3|
|Limit-Skip 5 : 2|
|Limit-Skip 6 : 1|
|Limit-Skip 7 : 0|


//@ Collection.Find Parameter Binding
|Find Binding Length: 1|
|Find Binding Name: alma|

//@ Collection.Find Field Selection Using Field List
|First Name: JACK|
|Age: 17|

//@ Collection.Find Field Selection Using Field Parameters
|First Name: JACK|
|Age: 17|

//@ Collection.Find Field Selection Using Projection Expression
|First Name: JACK|
|In Three Years: 20|
